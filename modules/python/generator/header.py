from typing import List, Optional, Set, Tuple, Dict, Union
from pathlib import Path
from dataclasses import dataclass
from collections import OrderedDict

import pcpp
from cxxheaderparser.parserstate import ClassBlockState, State
from cxxheaderparser import types
from cxxheaderparser.simple import parse_string, ParsedData, NamespaceScope, ClassScope

from utils import *
from methods import *
from doc_parser import *

from typing import TYPE_CHECKING
if TYPE_CHECKING:
  from submodule import Submodule

@dataclass
class BoundObjectNames:
  python_ident: str
  python_name: str
  cpp_no_template_name: str
  cpp_name: str

def filter_includes(include_names: Set[str]) -> List[str]:
  result = []
  for include_name in include_names:
    if include_name.startswith('"') and include_name.endswith('"'):
      continue
    forbidden_strs = ['winsock', '<io.h>']
    forbidden_found = False
    for forbidden_str in forbidden_strs:
      if forbidden_str in include_name:
        forbidden_found = True
        break
    if forbidden_found:
      continue

    result.append(include_name)
  return result

def sort_headers(headers: List['HeaderFile']) -> List['HeaderFile']:
  def add_level(result: List['HeaderFile'], remainder: List['HeaderFile'], dependencies: Set[str]):
    if len(remainder) == 0:
      return

    include_in_result_fn = None
    if len(result) == 0: # First iteration, query all headers that have no dependencies
      include_in_result_fn = lambda h: len(h.depends) == 0
    else:
      include_in_result_fn = lambda h: any(map(lambda x: x in dependencies, h.depends))
    new_remainder = []
    new_dependencies = []
    for header_file in remainder:
      has_dependency = include_in_result_fn(header_file)
      if has_dependency:
        new_dependencies.extend(header_file.contains)
        result.append(header_file)
      else:
        new_remainder.append(header_file)
    if new_remainder == remainder:
      print('Warning: Could not completely solve dependencies, generating but might have some errors')
      result.extend(remainder)
    else:
      add_level(result, new_remainder, set(new_dependencies))
  result = []
  add_level(result, headers, set())
  return result

class HeaderEnvironment():
  def __init__(self, data: ParsedData):
    self.mapping = self.build_naive_mapping(data.namespace)
    from enum_binding import resolve_enums_and_typedefs
    enum_reprs, _ = resolve_enums_and_typedefs(data.namespace, self.mapping)
    for enum_repr in enum_reprs:
      for value in enum_repr.values:
        self.mapping[value.name] = enum_repr.name + '::' + value.name

  def build_naive_mapping(self, data: Union[NamespaceScope, ClassScope], mapping={}, scope: str = ''):
    if isinstance(data, NamespaceScope):
      for alias in data.using_alias:
        mapping[alias.alias] = get_type(alias.type, {}, mapping)
      for typedef in data.typedefs:
        mapping[typedef.name] = scope + typedef.name

      for enum in data.enums:
        if not name_is_anonymous(enum.typename):
          enum_name = '::'.join([seg.name for seg in enum.typename.segments])
          mapping[enum_name] = scope + enum_name

      for cls in data.classes:
        cls_name = '::'.join([seg.name for seg in cls.class_decl.typename.segments])
        mapping[cls_name] = scope + cls_name
        mapping.update(self.build_naive_mapping(cls, mapping=mapping, scope=f'{scope}{cls_name}::'))
      for namespace in data.namespaces:
        mapping.update(self.build_naive_mapping(data.namespaces[namespace], mapping=mapping, scope=f'{scope}{namespace}::'))

    elif isinstance(data, ClassScope):
      for alias in data.using_alias:
        mapping[alias.alias] = get_type(alias.type, {}, mapping)
      for typedef in data.typedefs:
        mapping[typedef.name] = scope + typedef.name
      for enum in data.enums:
        if not name_is_anonymous(enum.typename):
          enum_name = '::'.join([seg.name for seg in enum.typename.segments])
          mapping[enum_name] = scope + enum_name

      for cls in data.classes:
        cls_name = '::'.join([seg.name for seg in cls.class_decl.typename.segments if not isinstance(seg, types.AnonymousName)])
        mapping[cls_name] = scope + cls_name
        mapping.update(self.build_naive_mapping(cls, mapping=mapping, scope=f'{scope}{cls_name}::'))
    return mapping

class HeaderFile():
  def __init__(self, path: Path, submodule: 'Submodule'):
    self.path = path
    self.submodule = submodule
    self.includes = [f'<visp3/{self.submodule.name}/{self.path.name}>']
    self.binding_code = None
    self.header_repr = None
    self.declarations = []
    self.contains = []
    self.depends = []
    self.documentation_holder_path: Path = None
    self.documentation_holder = None

  def __getstate__(self):
    return self.__dict__
  def __setstate__(self, d):
    self.__dict__ = d

  def preprocess(self) -> None:
    '''
    Preprocess the header to obtain the abstract representation of the cpp classes available.
    Additionally get the path to the xml documentation file generated by doxygen
    '''
    preprocessed_header_str = self.run_preprocessor() # Run preprocessor, get only code that can be compiled with current visp
    self.header_repr: ParsedData = parse_string(preprocessed_header_str) # Get the cxxheaderparser representation of the header

    # Get dependencies of this header. This is important for the code generation order
    for cls in self.header_repr.namespace.classes:
      name_cpp_no_template = '::'.join([seg.name for seg in cls.class_decl.typename.segments])
      self.contains.append(name_cpp_no_template)

      # Add parent classes as dependencies
      for base_class in cls.class_decl.bases:
        if base_class.access == 'public':
          base_class_str_no_template = '::'.join([segment.name for segment in base_class.typename.segments])
          if base_class_str_no_template.startswith('vp'):
              self.depends.append(base_class_str_no_template)

      # Get documentation if available, only one document supported for now
      if self.documentation_holder_path is None:
        self.documentation_holder_path = DocumentationData.get_xml_path_if_exists(name_cpp_no_template, DocumentationObjectKind.Class)

  def run_preprocessor(self): # TODO: run without generating a new file
    tmp_dir = self.submodule.submodule_file_path.parent / "tmp"
    tmp_dir.mkdir(exist_ok=True)
    tmp_file_path = tmp_dir / self.path.name
    argv = [
      '',
      '-D', 'vp_deprecated=',
      '-D', 'VISP_EXPORT=',
      '-D', 'DOXYGEN_SHOULD_SKIP_THIS', # Skip methods and classes that are not exposed in documentation: they are internals
      '-I', '/home/sfelton/software/visp_build/include',
      '-I', '/usr/local/include',
      #'-I', '/usr/include',
      '-N', 'VISP_BUILD_DEPRECATED_FUNCTIONS',
      '--passthru-includes', "^((?!vpConfig.h).)*$",
      '--passthru-unfound-includes',
      '--passthru-comments',
      '--line-directive', '',
      '-o', f'{tmp_file_path}',
      str(self.path.absolute())
    ]
    pcpp.CmdPreprocessor(argv)
    preprocessed_header_content = None
    with open(tmp_file_path, 'r') as header_file:
      preprocessed_header_content = '\n'.join(header_file.readlines())
      preprocessed_header_content = preprocessed_header_content.replace('#include<', '#include <') # Bug in cpp header parser
    return preprocessed_header_content

  def generate_binding_code(self) -> None:
    assert self.header_repr is not None, 'The header was not preprocessed before calling the generation step!'

    self.binding_code = self.parse_data()


  def parse_data(self):
    from enum_binding import enum_bindings
    result = ''
    print(f'Building environment for {self.path}')
    header_env = HeaderEnvironment(self.header_repr)
    if 'AprilTag' in self.path.name:
      import pprint
      pprint.pprint(header_env.mapping)
    if self.documentation_holder_path is not None:
      self.documentation_holder = DocumentationHolder(self.documentation_holder_path, header_env.mapping)
    else:
      print(f'No documentation found for header {self.path}')
    for cls in self.header_repr.namespace.classes:
      result += self.generate_class(cls, header_env) + '\n'
    enum_decls_and_bindings = enum_bindings(self.header_repr.namespace, header_env.mapping, self.submodule)
    for declaration, binding in enum_decls_and_bindings:
      self.declarations.append(declaration)
      result += binding
    return result

  def generate_class(self, cls: ClassScope, header_env: HeaderEnvironment) -> str:
    def generate_class_with_potiental_specialization(name_python: str, owner_specs: OrderedDict[str, str], cls_config: Dict) -> str:
      python_ident = f'py{name_python}'
      name_cpp = get_typename(cls.class_decl.typename, owner_specs, header_env.mapping)
      class_doc = None
      if self.documentation_holder is not None:
        class_doc = self.documentation_holder.get_documentation_for_class(name_cpp_no_template, {}, owner_specs)
      else:
        print(f'Documentation not found when looking up {name_cpp_no_template}')
      # Declaration
      # Add template specializations to cpp class name. e.g., vpArray2D becomes vpArray2D<double> if the template T is double
      template_decl: Optional[types.TemplateDecl] = cls.class_decl.template
      if template_decl is not None:
        template_strs = []
        template_strs = map(lambda t: owner_specs[t.name], template_decl.params)
        template_str = f'<{", ".join(template_strs)}>'
        name_cpp += template_str

      # Reference base classes when creating pybind class binding
      base_class_strs = map(lambda base_class: get_typename(base_class.typename, owner_specs, header_env.mapping),
                            filter(lambda b: b.access == 'public', cls.class_decl.bases))
      class_template_str = ', '.join([name_cpp] + list(base_class_strs))
      doc_param = [] if class_doc is None else [class_doc.documentation]

      cls_argument_strs = ['submodule', f'"{name_python}"'] + doc_param + (['py::buffer_protocol()'] if cls_config['use_buffer_protocol'] else [])

      class_decl = f'\tpy::class_ {python_ident} = py::class_<{class_template_str}>({", ".join(cls_argument_strs)});'
      self.declarations.append(class_decl)

      # Definitions
      # Skip constructors for classes that have pure virtual methods since they cannot be instantiated
      contains_pure_virtual_methods = False
      for method in cls.methods:
        if method.pure_virtual:
          contains_pure_virtual_methods = True
          break
      # User marked this class as virtual.
      # This is required if no virtual method is declared in this class,
      #  but it does not implement pure virtual methods of a base class
      contains_pure_virtual_methods = contains_pure_virtual_methods or cls_config['is_virtual']

      # Find bindable methods
      generated_methods = []
      method_strs = []
      bindable_methods_and_config, rejected_methods = get_bindable_methods_with_config(self.submodule, cls.methods,
                                                                                    name_cpp_no_template, owner_specs, header_env.mapping)
      # Display rejected methods
      rejection_strs = []
      for rejected_method in rejected_methods:
        self.submodule.report.add_non_generated_method(rejected_method)
        if NotGeneratedReason.is_non_trivial_reason(rejected_method.rejection_reason):
          rejection_strs.append(f'\t{rejected_method.signature} was rejected! Reason: {rejected_method.rejection_reason}')
      if len(rejection_strs) > 0:
        print(f'Rejected method in class: {name_cpp}')
        print('\n'.join(rejection_strs))

      # Split between constructors and other methods
      constructors, non_constructors = split_methods_with_config(bindable_methods_and_config, lambda m: m.constructor)

      # Split between "normal" methods and operators, which require a specific definition
      cpp_operator_names = cpp_operator_list()
      operators, basic_methods = split_methods_with_config(non_constructors, lambda m: get_name(m.name) in cpp_operator_names)

      # Constructors definitions
      if not contains_pure_virtual_methods:
        for method, method_config in constructors:
          params_strs = [get_type(param.type, owner_specs, header_env.mapping) for param in method.parameters]
          py_arg_strs = get_py_args(method.parameters, owner_specs, header_env.mapping)
          ctor_str = f'''{python_ident}.{define_constructor(params_strs, py_arg_strs)};'''
          method_strs.append(ctor_str)

      # Operator definitions
      binary_return_ops = supported_const_return_binary_op_map()
      binary_in_place_ops = supported_in_place_binary_op_map()

      for method, method_config in operators:
        method_name = get_name(method.name)
        method_is_const = method.const
        params_strs = [get_type(param.type, owner_specs, header_env.mapping) for param in method.parameters]
        return_type_str = get_type(method.return_type, owner_specs, header_env.mapping)
        py_args = get_py_args(method.parameters, owner_specs, header_env.mapping)
        py_args = py_args + ['py::is_operator()']
        if len(params_strs) > 1:
          print(f'Found operator {name_cpp}{method_name} with more than one parameter, skipping')
          rejection = RejectedMethod(name_cpp, method, method_config, get_method_signature(method_name, return_type_str, params_strs), NotGeneratedReason.NotHandled)
          self.submodule.report.add_non_generated_method(rejection)
          continue
        elif len(params_strs) < 1:
          print(f'Found unary operator {name_cpp}::{method_name}, skipping')
          continue
        for cpp_op, python_op_name in binary_return_ops.items():
          if method_name == f'operator{cpp_op}':
            operator_str = f'''
{python_ident}.def("__{python_op_name}__", []({"const" if method_is_const else ""} {name_cpp}& self, {params_strs[0]} o) {{
  return (self {cpp_op} o);
}}, {", ".join(py_args)});'''
            method_strs.append(operator_str)
            break
        for cpp_op, python_op_name in binary_in_place_ops.items():
          if method_name == f'operator{cpp_op}':
            operator_str = f'''
{python_ident}.def("__{python_op_name}__", []({"const" if method_is_const else ""} {name_cpp}& self, {params_strs[0]} o) {{
  self {cpp_op} o;
  return self;
}}, {", ".join(py_args)});'''
            method_strs.append(operator_str)
            break

      # Define classical methods
      class_def_names = BoundObjectNames(python_ident, name_python, name_cpp_no_template, name_cpp)
      for method, method_config in basic_methods:
        if method.template is not None and method_config.get('specializations') is not None:
          method_template_names = [t.name for t in method.template.params]
          specializations = method_config.get('specializations')
          for method_spec in specializations:
            new_specs = owner_specs.copy()
            assert len(method_template_names) == len(method_spec)
            method_spec_dict = OrderedDict(k for k in zip(method_template_names, method_spec))
            new_specs.update(method_spec_dict)
            method_str, generated_method_tuple = define_method(method, method_config, True,
                                                                new_specs, self, header_env, class_def_names)
            method_strs.append(method_str)
            generated_methods.append(generated_method_tuple)
        else:
          method_str, generated_method_tuple = define_method(method, method_config, True,
                                                              owner_specs, self, header_env, class_def_names)
          method_strs.append(method_str)
          generated_methods.append(generated_method_tuple)

      # Add to string representation
      if not cls_config['ignore_repr']:
        to_string_str = find_and_define_repr_str(cls, name_cpp, python_ident)
        if len(to_string_str) > 0:
          method_strs.append(to_string_str)

      # Add call to user defined bindings function
      # Binding function should be defined in the static part of the generator
      # It should have the signature void fn(py::class_<Type>& cls);
      # If it is for a templated class, it should also be templated in the same way (same order of parameters etc.)
      if cls_config['additional_bindings'] is not None:
        template_str = ''
        if len(owner_specs.keys()) > 0:
          template_types = owner_specs.values()
          template_str = f'<{", ".join([template_type for template_type in template_types])}>'

        method_strs.append(f'{cls_config["additional_bindings"]}({python_ident});')


      # Check for potential error-generating definitions
      error_generating_overloads = get_static_and_instance_overloads(generated_methods)
      for error_overload in error_generating_overloads:
        print(f'Overload {error_overload} defined for instance and class, this will generate a pybind error')
        for method_str in method_strs:
          if error_overload in method_str:
            print(method_str)
        print()
      if len(error_generating_overloads) > 0:
        print(error_generating_overloads)
        raise RuntimeError

      #spec_result += cls_result
      return '\n'.join(method_strs)


    name_cpp_no_template = '::'.join([seg.name for seg in cls.class_decl.typename.segments])
    print(f'Parsing class "{name_cpp_no_template}"')

    if self.submodule.class_should_be_ignored(name_cpp_no_template):
      self.submodule.report.add_non_generated_class(name_cpp_no_template, {}, 'Skipped by user')
      return ''

    cls_config = self.submodule.get_class_config(name_cpp_no_template)
    if cls.class_decl.template is None:
      name_python = name_cpp_no_template.replace('vp', '')
      return generate_class_with_potiental_specialization(name_python, {}, cls_config)
    else:
      if cls_config is None or 'specializations' not in cls_config or len(cls_config['specializations']) == 0:
        print(f'Could not find template specialization for class {name_cpp_no_template}: skipping!')
        self.submodule.report.add_non_generated_class(name_cpp_no_template, cls_config, 'Skipped because there was no declared specializations')
        return ''
      else:
        specialization_strs = []
        specs = cls_config['specializations']
        template_names = [t.name for t in cls.class_decl.template.params]
        for spec in specs:
          name_python = spec['python_name']
          args = spec['arguments']
          assert len(template_names) == len(args), f'Specializing {name_cpp_no_template}: Template arguments are {template_names} but found specialization {args} which has the wrong number of arguments'
          spec_dict = OrderedDict(k for k in zip(template_names, args))
          specialization_strs.append(generate_class_with_potiental_specialization(name_python, spec_dict, cls_config))

        return '\n'.join(specialization_strs)

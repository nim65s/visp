from typing import List
import sys
from pathlib import Path
from multiprocessing import Pool
import argparse

from visp_python_bindgen.header import *
from visp_python_bindgen.submodule import *


def header_preprocess(header: HeaderFile):
  '''
  Preprocess a single header. Supposed to be called from a subprocess.
  '''
  try:
    header.preprocess()
    return header
  except Exception as e:
    print('There was an error when processing header', header.path)
    import traceback
    traceback.print_exc()
    return None

def main_str(submodule_fn_declarations, submodule_fn_calls):
  '''
  Main binding generation content
  :param submodule_fn_declarations: declaration of the functions that bind the submodules
  :param submodule_fn_calls: Calls to the functions declared above

  '''
  return f'''
//#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/pybind11.h>
namespace py = pybind11;
{submodule_fn_declarations}

PYBIND11_MODULE(_visp, m)
{{
  m.doc() = "ViSP Python binding";

  {submodule_fn_calls}
}}
'''

def generate_module(generate_path: Path, config_path: Path) -> None:

  submodules: List[Submodule] = get_submodules(config_path, generate_path)

  # Step 1: Preprocess all headers
  all_headers: List[HeaderFile] = []
  for submodule in submodules:
    all_headers.extend(submodule.headers)

  # Parallel processing of headers to speedup this step
  from tqdm import tqdm
  with Pool() as pool:
    new_all_headers = []
    for result in list(tqdm(pool.imap(header_preprocess, all_headers), total=len(all_headers), file=sys.stderr)):
      if result is None:
        raise RuntimeError('There was an exception when processing headers: You should either ignore them, ignore the failing class, or fix the generator code!')
      new_all_headers.append(result)

  # Sort headers according to the dependencies. This is done across all modules.
  # TODO: sort module generation order. For now this works but it's fairly brittle
  new_all_headers = sort_headers(new_all_headers)
  for header in new_all_headers:
    header.compute_environment()

  headers_with_deps = list(map(lambda header: (header, header.get_header_dependencies(new_all_headers)), new_all_headers))

  for header, header_deps in headers_with_deps:
    other_mappings = list(map(lambda h: h.environment.mapping, header_deps))
    header.environment.update_with_dependencies(other_mappings)

  for submodule in submodules:
    submodule.set_headers_from_common_list(new_all_headers)

  # Step 2: generate the binding code for each submodule
  # Each submodule write to its own file(s) its binding code
  for submodule in submodules:
    submodule.generate()

  # Step 3: write to main.cpp the call to the submodule binding implementations.
  main_path = generate_path / 'main.cpp'
  with open(main_path, 'w') as main_file:
    submodule_fn_declarations = []
    submodule_fn_calls = []
    for submodule in submodules:
      name = submodule.generation_function_name()
      submodule_fn_declarations.append(f'void {name}(py::module_&);')
      submodule_fn_calls.append(f'{name}(m);')

    submodule_fn_declarations = '\n'.join(submodule_fn_declarations)
    submodule_fn_calls = '\n'.join(submodule_fn_calls)

    format_str = main_str(submodule_fn_declarations, submodule_fn_calls)
    main_file.write(format_str)

def main():
  parser = argparse.ArgumentParser('Python Bindings generator for ViSP')
  parser.add_argument('--config', type=str, required=True)
  parser.add_argument('--build-folder', type=str, required=True)
  args = parser.parse_args()

  generation_path = Path(args.build_folder)
  assert generation_path.exists(), f'Path to where to generate bindings does not exist!Path: {generation_path}'
  generation_path = generation_path / 'src'
  generation_path.mkdir(exist_ok=True)

  config_path = Path(args.config)
  assert config_path.exists(), f'Path to the folder containing the configuration files does not exist! Path: {config_path}'

  generate_module(generation_path, config_path)

if __name__ == '__main__':
  main()
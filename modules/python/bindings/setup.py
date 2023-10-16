import os
import re
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext
from setuptools.command.install import install
from distutils.command import build as build_module
from pathlib import Path

package_name = 'visp'
version = '0.0.4'

package_data = {}

if os.name == 'posix':
    package_data[package_name] = ['*.so']
else:
    package_data[package_name] = ['*.pyd', '*.dll']


# This creates a list which is empty but returns a length of 1.
# Should make the wheel a binary distribution and platlib compliant.
class EmptyListWithLength(list):
    def __len__(self):
        return 1

# The information here can also be placed in setup.cfg - better separation of
# logic and declaration, and simpler if you include description/version in a file.
setup(
    name=package_name,
    version=version,
    author="Samuel Felton",
    author_email="samuel.felton@irisa.fr",
    description="Python wrapper for the Visual Servoing Platform",
    long_description="",
    setup_requires=[],
    ext_modules=EmptyListWithLength,
    zip_safe=False,
    include_package_data=True,
    package_data=package_data,
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.7",
)

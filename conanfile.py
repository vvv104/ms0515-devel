"""
Conan 2 recipe for the RT-11 development repository: the programs' harnesses
built together with the emulator they run on.

Build with: conan build .

The packages are the emulator's: its own recipe (src/conanfile.py in its
repository, $MS0515_DIR, else ../ms0515) says what they are, and this one
asks for the same, so the two never drift apart.
"""

import importlib.util
import os

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

HERE = os.path.dirname(os.path.abspath(__file__))
EMULATOR = os.environ.get("MS0515_DIR") or os.path.join(os.path.dirname(HERE), "ms0515")


def _emulator_recipe():
    path = os.path.join(EMULATOR, "src", "conanfile.py")
    if not os.path.isfile(path):
        raise RuntimeError(f"no emulator repository at {EMULATOR}: set MS0515_DIR or run tools/deps.py")
    spec = importlib.util.spec_from_file_location("ms0515_recipe", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.Ms0515Recipe


Emulator = _emulator_recipe()


class Ms0515DevelRecipe(ConanFile):
    name            = "ms0515-devel"
    settings        = "os", "arch", "compiler", "build_type"
    generators      = "CMakeDeps", "CMakeToolchain"
    default_options = Emulator.default_options

    def build_requirements(self):
        Emulator.build_requirements(self)

    def requirements(self):
        Emulator.requirements(self)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={"MS0515_DIR": EMULATOR.replace("\\", "/")})
        cmake.build()

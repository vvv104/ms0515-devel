# The harnesses of the programs under projects/ (and of the GCC toolset):
# each boots or runs a built program through the emulator's library and
# checks it, the program's own tests/.  The emulator is built here as a
# part of this build, from its repository ($MS0515_DIR, else ../ms0515 -
# toolset/cmake/Ms0515.cmake), so the harnesses link its targets; its
# package/ is left alone.  Each harness skips itself when its program is
# not built.
#
#     conan build .        (conanfile.py: the emulator's packages, then this)
cmake_minimum_required(VERSION 3.21)
project(ms0515_devel LANGUAGES C CXX)

# ── Language standard ───────────────────────────────────────────────────────
# Declared once for this repository's targets; the emulator declares its own
# for its own.  The Conan profiles' compiler.cppstd must match.
set(CMAKE_C_STANDARD              11)
set(CMAKE_C_STANDARD_REQUIRED     ON)
set(CMAKE_CXX_STANDARD            20)
set(CMAKE_CXX_STANDARD_REQUIRED   ON)
set(CMAKE_CXX_EXTENSIONS          OFF)

include(toolset/cmake/Ms0515.cmake)
if(NOT EXISTS "${MS0515_DIR}/src/CMakeLists.txt")
    message(FATAL_ERROR "no emulator repository at ${MS0515_DIR}: "
                        "set MS0515_DIR or run tools/deps.py")
endif()

# The emulator: its libraries and tools, no window, no tests of its own.
set(MS0515_BUILD_FRONTEND OFF)
set(MS0515_BUILD_TESTS    OFF)
add_subdirectory("${MS0515_DIR}/src" ms0515)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_subdirectory(projects/fist/tests)
add_subdirectory(projects/manicm/tests)
add_subdirectory(projects/rt11/handlers/sl/tests)
add_subdirectory(projects/sl/tests)
add_subdirectory(projects/minesweeper/tests)
add_subdirectory(toolset/gcc/tests)
add_subdirectory(projects/stars/tests)
add_subdirectory(projects/unix/tests)

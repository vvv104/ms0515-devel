# CRYPT - a dungeon crawl in the first person, in C for GCC
# (rt11_devel/toolset/gcc): compiled on the host, run by ms0515-run.
#
#     cmake -S rt11_devel/projects/crypt -B <build folder> -G Ninja
#     cmake --build <build folder>
#
# The program comes out as <build folder>/sav/CRYPT.SAV; `ms0515-run CRYPT`
# there plays it.  The art is PNGs under art/, turned into C by
# tools/mkart.py at build time (the stand-ins are drawn by
# art/placeholders.py); the view's geometry is tables made by
# art/geometry.py.  The compiler: $MS0515_GCC/bin/pdp11-aout-gcc, or on
# the PATH (rt11_devel/toolset/gcc/README.md).

cmake_minimum_required(VERSION 3.21)
include(../../toolset/cmake/Rt11Gcc.cmake)
project(crypt LANGUAGES C ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools")
set(MKART "${TOOLS_DIR}/mkart.py")
set(ART_DIR "${CMAKE_CURRENT_BINARY_DIR}/art")
file(MAKE_DIRECTORY "${ART_DIR}")

# art(<kind> <name> <png> [mkart switches]): art/<png>.png as C under the
# name, appended to ART.
set(ART "")
function(art kind name png)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/art/${png}.png")
    set(c "${ART_DIR}/${name}.c")
    add_custom_command(
        OUTPUT  "${c}"
        COMMAND ${Python3_EXECUTABLE} "${MKART}" ${kind} ${name} "${source}" "${c}" ${ARGN}
        DEPENDS "${source}" "${MKART}"
        COMMENT "${png}.png as the ${kind} ${name}"
        VERBATIM)
    set(ART ${ART} "${c}" PARENT_SCOPE)
endfunction()

art(tex skeleton skeleton --key 0,0,0)
art(image face1 face1)
art(image face2 face2)
art(image face3 face3)
art(image face4 face4)

# The view's geometry: the tables of where every cell corner and every
# wall's edge falls, as a header.
set(GEOMETRY "${CMAKE_CURRENT_SOURCE_DIR}/art/geometry.py")
set(GEOMETRY_H "${ART_DIR}/geometry.h")
add_custom_command(
    OUTPUT  "${GEOMETRY_H}"
    COMMAND ${Python3_EXECUTABLE} "${GEOMETRY}" "${GEOMETRY_H}"
    DEPENDS "${GEOMETRY}"
    COMMENT "the view's geometry: geometry.h"
    VERBATIM)
add_custom_target(geometry DEPENDS "${GEOMETRY_H}")

rt11_c_library(NAME art SOURCES ${ART} INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}")
rt11_c_library(NAME engine SOURCES src/map.c src/view.c src/hud.c INCLUDE src "${ART_DIR}")
target_link_libraries(engine PUBLIC art)		# the screen round the view draws the party's faces
add_dependencies(engine geometry)
rt11_c_program(NAME CRYPT SOURCES src/main.c LIBRARIES engine STACK 1536)

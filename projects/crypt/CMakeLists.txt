# CRYPT - a dungeon crawl in the first person, in C for GCC
# (rt11_devel/toolset/gcc): compiled on the host, run by ms0515-run.
#
#     cmake -S rt11_devel/projects/crypt -B <build folder> -G Ninja
#     cmake --build <build folder>
#
# The program comes out as <build folder>/sav/CRYPT.SAV; `ms0515-run CRYPT`
# there plays it.  The art is PNGs under art/, turned into C by
# tools/mkart.py at build time (the stand-ins are drawn by
# art/placeholders.py).  The compiler: $MS0515_GCC/bin/pdp11-aout-gcc, or
# on the PATH (rt11_devel/toolset/gcc/README.md).

cmake_minimum_required(VERSION 3.21)
include(../../toolset/cmake/Rt11Gcc.cmake)
project(crypt LANGUAGES C ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(MKART "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/mkart.py")
set(ART_DIR "${CMAKE_CURRENT_BINARY_DIR}/art")
file(MAKE_DIRECTORY "${ART_DIR}")

# art(<kind> <name> [--key R,G,B]): art/<name>.png as C, appended to ART.
set(ART "")
function(art kind name)
    set(png "${CMAKE_CURRENT_SOURCE_DIR}/art/${name}.png")
    set(c "${ART_DIR}/${name}.c")
    add_custom_command(
        OUTPUT  "${c}"
        COMMAND ${Python3_EXECUTABLE} "${MKART}" ${kind} ${name} "${png}" "${c}" ${ARGN}
        DEPENDS "${png}" "${MKART}"
        COMMENT "${name}.png as a ${kind}"
        VERBATIM)
    set(ART ${ART} "${c}" PARENT_SCOPE)
endfunction()

art(tex wall)
art(tex skeleton --key 0,0,0)
art(image face1)
art(image face2)
art(image face3)
art(image face4)

rt11_c_library(NAME art SOURCES ${ART} INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}")
rt11_c_library(NAME engine SOURCES src/map.c src/view.c src/hud.c INCLUDE src)
target_link_libraries(engine PUBLIC art)		# the view draws the wall's texture
rt11_c_program(NAME CRYPT SOURCES src/main.c LIBRARIES engine STACK 2048)

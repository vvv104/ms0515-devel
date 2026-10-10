# CRYPT - a dungeon crawl in the first person, in C for GCC
# (rt11_devel/toolset/gcc): compiled on the host, run by ms0515-run.
#
#     cmake -S rt11_devel/projects/crypt -B <build folder> -G Ninja
#     cmake --build <build folder>
#
# The program comes out as <build folder>/sav/CRYPT.SAV with CRYPT.DAT
# beside it; `ms0515-run CRYPT` there plays it.  The art is PNGs under
# art/, turned into C by tools/mkart.py at build time (the stand-ins are
# drawn by art/placeholders.py); the walls along the party's axis are
# drawn in perspective by art/planes.py into CRYPT.DAT, which the
# program loads into the bank above itself - so the program must end
# under 060000 (LIMIT).  The compiler: $MS0515_GCC/bin/pdp11-aout-gcc,
# or on the PATH (rt11_devel/toolset/gcc/README.md).

cmake_minimum_required(VERSION 3.21)
include(../../toolset/cmake/Rt11Gcc.cmake)
project(crypt LANGUAGES C ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools")
set(MKART "${TOOLS_DIR}/mkart.py")
set(ART_DIR "${CMAKE_CURRENT_BINARY_DIR}/art")
set(SAV_DIR "${CMAKE_CURRENT_BINARY_DIR}/sav")
file(MAKE_DIRECTORY "${ART_DIR}" "${SAV_DIR}")

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

art(tex wall wall)
art(tex wall2 wall --shrink 2)
art(tex wall4 wall --shrink 4)
art(tex skeleton skeleton --key 0,0,0)
art(image face1 face1)
art(image face2 face2)
art(image face3 face3)
art(image face4 face4)

# The side walls in perspective: CRYPT.DAT beside the program, and the
# header that says where in it what lies, with the geometry tables.
set(PLANES "${CMAKE_CURRENT_SOURCE_DIR}/art/planes.py")
set(PLANES_H "${ART_DIR}/planes.h")
add_custom_command(
    OUTPUT  "${SAV_DIR}/CRYPT.DAT" "${PLANES_H}"
    COMMAND ${Python3_EXECUTABLE} "${PLANES}" "${CMAKE_CURRENT_SOURCE_DIR}/art/wall.png"
            "${SAV_DIR}/CRYPT.DAT" "${PLANES_H}" --mkart "${TOOLS_DIR}"
    DEPENDS "${PLANES}" "${CMAKE_CURRENT_SOURCE_DIR}/art/wall.png" "${MKART}"
    COMMENT "the side walls in perspective: CRYPT.DAT"
    VERBATIM)
add_custom_target(planes ALL DEPENDS "${SAV_DIR}/CRYPT.DAT" "${PLANES_H}")

rt11_c_library(NAME art SOURCES ${ART} INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}")
rt11_c_library(NAME engine SOURCES src/map.c src/view.c src/hud.c src/data.c INCLUDE src "${ART_DIR}")
target_link_libraries(engine PUBLIC art)		# the view draws the wall's texture
add_dependencies(engine planes)
rt11_c_program(NAME CRYPT SOURCES src/main.c LIBRARIES engine STACK 1536 LIMIT 060000)

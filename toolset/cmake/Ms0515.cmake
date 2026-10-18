# Ms0515.cmake - where this repository finds what it stands on (the CMake
# side of toolset/ms0515.py).  Each a cache variable that an environment
# variable of the same name presets:
#
#   MS0515_DIR        the emulator's repository; ../ms0515 beside this one
#                     when not said (tools/deps.py puts it there)
#   MS0515_PACKAGE    its binaries; MS0515_DIR/package when not said (the
#                     emulator's own build puts them there)
#   MS0515_SOFTWARE   the software collection; ../ms0515-software beside
#                     this one when not said

include_guard(GLOBAL)

get_filename_component(MS0515_DEVEL "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(_ms0515_path var default doc)
    if(NOT ${var})
        if(DEFINED ENV{${var}})
            file(TO_CMAKE_PATH "$ENV{${var}}" value)
        else()
            get_filename_component(value "${default}" ABSOLUTE)
        endif()
        set(${var} "${value}" CACHE PATH "${doc}")
    endif()
endfunction()

_ms0515_path(MS0515_DIR "${MS0515_DEVEL}/../ms0515" "The emulator's repository")
_ms0515_path(MS0515_PACKAGE "${MS0515_DIR}/package" "The emulator's binaries")
_ms0515_path(MS0515_SOFTWARE "${MS0515_DEVEL}/../ms0515-software" "The software collection")

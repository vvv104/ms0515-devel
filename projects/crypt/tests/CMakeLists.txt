# ── CRYPT tests ──────────────────────────────────────────────────────────────
# The game's own harness: CRYPT.SAV run on the machine of ms0515-run,
# headlessly - the screen read out of video memory, the keys pressed, the
# pace read from what the program prints on its way out.  Tests of the
# game, not of the emulator, which is why they live here and not under
# src/.
#
# The program is the project's build in rt11_devel/projects/crypt/build
# (see ../CMakeLists.txt); another folder is named on the command line:
# crypt_tests --crypt-sav=<folder>, and then a missing program is a
# failure.  Everything skips itself when the default folder has none.
# Without ms0515-run's machine (it needs the software collection) the
# harness is left out.
# ─────────────────────────────────────────────────────────────────────────────

if(NOT TARGET ms0515_run_core)
    message(STATUS "crypt_tests: no ms0515-run machine - not built")
    return()
endif()

find_package(doctest REQUIRED)

add_executable(crypt_tests
    test_main.cpp
    test_crypt.cpp
)

target_include_directories(crypt_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/src
)

target_compile_definitions(crypt_tests PRIVATE
    CRYPT_SAV_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../build/sav"
)

target_link_libraries(crypt_tests
    PRIVATE
        ms0515_run_core
        ms0515_disk          # EmulatorInternal.hpp reaches the disk lib's headers
        ms0515_libapp        # the screen as a PNG, for looking at what a test saw
        doctest::doctest
)

add_test(NAME crypt_tests COMMAND crypt_tests)

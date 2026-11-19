# ── SAPER tests ──────────────────────────────────────────────────────────────
# The game's own test harness: boots the built SAPER.SAV with its SAPER.HLP
# and SAPER.DAT headlessly through ms0515_lib (the emulator's public API) and
# checks the game from the outside, reading its state where the link map
# (SAPER.MAP) puts it.  These are tests of the GAME, not of the emulator,
# which is why they live here and not under src/.  Everything skips itself
# when the game is not built (python toolset/build.py projects/saper/build.toml).
# ─────────────────────────────────────────────────────────────────────────────

find_package(doctest REQUIRED)

add_executable(saper_tests
    test_main.cpp
    test_game.cpp
)

target_include_directories(saper_tests PRIVATE
    ${ms0515_SOURCE_DIR}/lib/src
)

target_compile_definitions(saper_tests PRIVATE
    ASSETS_DIR="${ms0515_SOURCE_DIR}/assets"
    SAPER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    SAPER_SYSTEM_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../toolset/system"
)

target_link_libraries(saper_tests
    PRIVATE
        ms0515_lib
        ms0515_disk          # EmulatorInternal.hpp reaches the disk lib's headers
        doctest::doctest
)

add_test(NAME saper_tests COMMAND saper_tests)

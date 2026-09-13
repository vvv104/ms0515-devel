# ── CELLWAR tests ────────────────────────────────────────────────────────────
# The game's own test harness: boots the built CELWAR.SAV headlessly
# through ms0515_lib (the emulator's public API) and checks the rules from
# the outside - a position poked onto the board, the keys pressed, the
# board read back - where the link map (CELWAR.MAP) puts the state.  These
# are tests of the GAME, not of the emulator, which is why they live here
# and not under src/.  Everything skips itself when the game is not built.
# ─────────────────────────────────────────────────────────────────────────────

find_package(doctest REQUIRED)

add_executable(cellwar_tests
    test_main.cpp
    test_rules.cpp
)

target_include_directories(cellwar_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/src
)

target_compile_definitions(cellwar_tests PRIVATE
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    CELLWAR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    CELLWAR_SYSTEM_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../toolset/system"
)

target_link_libraries(cellwar_tests
    PRIVATE
        ms0515_lib
        ms0515_disk          # EmulatorInternal.hpp reaches the disk lib's headers
        doctest::doctest
)

add_test(NAME cellwar_tests COMMAND cellwar_tests)

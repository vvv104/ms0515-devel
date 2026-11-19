/*
 * test_game.cpp - SAPER booted through RT-11 on a folder device and
 * checked from outside: the cursor on the board, an arrow tapped, an
 * arrow held.  Every test skips when the game is not built.
 */
#include "SaperGame.hpp"

using saper::SaperGame;
using ms0515::Key;

TEST_CASE("saper: the board is up with the cursor on it")
{
    if (!saper::built()) { MESSAGE("SAPER not built - skipped"); return; }
    SaperGame g("saper_start");
    CHECK(g.word("CURX") >= g.word("XMIN"));
    CHECK(g.word("CURX") <= g.word("XMAX"));
}

TEST_CASE("saper: an arrow tapped moves the cursor one cell")
{
    if (!saper::built()) { MESSAGE("SAPER not built - skipped"); return; }
    SaperGame g("saper_tap");
    const int x0 = g.word("CURX");
    REQUIRE(x0 < g.word("XMAX"));
    g.keyTap(Key::Right, 4);                       // let go before the keyboard repeats
    g.settle(30);
    CHECK(g.word("CURX") == x0 + 1);
}

TEST_CASE("saper: an arrow held walks the cursor to the board's edge")
{
    /* The keyboard sends the arrow's code once, then 264 - the metronome -
     * while it stays down: the cursor has to walk on the 264s. */
    if (!saper::built()) { MESSAGE("SAPER not built - skipped"); return; }
    SaperGame g("saper_hold");
    const int x0 = g.word("CURX");
    REQUIRE(g.word("XMAX") - x0 >= 3);
    g.emu.keyPress(Key::Right, true);
    g.settle(60);                                  // over a second of repeats
    g.emu.keyPress(Key::Right, false);
    g.settle(10);
    CHECK(g.word("CURX") == g.word("XMAX"));
}

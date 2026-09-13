/*
 * test_title.cpp - the title, the demo and the grid's edges.
 *
 * The game boots to its title; SPACE starts a game, Q leaves, and left
 * alone it plays itself.  The grid closes on the right and below where
 * the view ends.  Every test skips when the game is not built.
 */
#include "CellwarGame.hpp"

using cellwar::CellwarGame;
using cellwar::Unit;
using cellwar::index;
using ms0515::Key;

TEST_CASE("cellwar title: the title is up, SPACE starts the game")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_title", true, 200);
    g.awaitTitle();
    CHECK(g.peek16("MODE") == 0);
    CHECK(g.vramNonzero() > 2000);                              // the logo and the field
    g.tap(Key::Space, 60);
    CHECK(g.peek16("MODE") == 2);
    CHECK(g.peek16("ROUND") == 1);
    CHECK(g.sel() == index(4, 4));
}

TEST_CASE("cellwar title: left alone the computer plays itself, and SPACE takes over")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_demo", true, 200);
    g.awaitTitle();
    REQUIRE(g.peek16("MODE") == 0);
    g.settle(600);
    CHECK(g.peek16("MODE") == 1);
    bool played = false;
    for (int i = 0; i < 3000 && !played; ++i) { g.step(); played = g.peek16("ROUND") >= 3; }
    CHECK(played);                                              // both sides move, round on round
    CHECK(g.count(false) >= 1);
    CHECK(g.count(true) >= 1);
    g.tap(Key::Space, 200);
    CHECK(g.peek16("MODE") == 2);
    CHECK(g.peek16("ROUND") == 1);                              // a fresh game
    CHECK(g.count(false) == 1);
}

TEST_CASE("cellwar title: Q at the title leaves for RT-11")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_titleq", true, 200);
    g.awaitTitle();
    REQUIRE(g.peek16("MODE") == 0);
    CHECK((g.board().reg_c & 010) == 0);
    g.tap(Key::Q, 60);
    CHECK((g.board().reg_c & 010) != 0);                        // the console's mode again
}

TEST_CASE("cellwar: the grid closes on the right and below where the view ends")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_edges");
    // the new game: the fortification at (4,4) sees x 2..6, y 2..6
    CHECK((g.pixels(6, 4, 8, true) & 1) != 0);                  // (7,4) is fog: closed
    CHECK((g.pixels(5, 4, 8, true) & 1) == 0);                  // (6,4) is seen: open
    CHECK(g.pixels(4, 6, 15, false) == 0xFF);                   // (4,7) is fog: closed
    CHECK(g.pixels(4, 6, 15, true) == 0xFF);
    CHECK(g.pixels(4, 5, 15, false) == 0x80);                   // (4,6) is seen: the left line only
    CHECK(g.pixels(2, 4, 0, false) == 0xFF);                    // the top and left lines as before
}

TEST_CASE("cellwar title: the demo comes to blows")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_demofight", true, 200);
    g.awaitTitle();
    g.settle(600);
    REQUIRE(g.peek16("MODE") == 1);
    int human = g.count(false), cpu = g.count(true);
    bool blood = false;
    for (int i = 0; i < 12000 && !blood; ++i) {
        g.step();
        if (g.peek16("MODE") != 1) break;                       // over: the title again
        const int h = g.count(false), c = g.count(true);
        blood = h < human || c < cpu;
        human = h > human ? h : human;
        cpu = c > cpu ? c : cpu;
    }
    CHECK(blood);                                               // somebody died
    CHECK(g.peek16("ROUND") < 30);                              //   and not late
}

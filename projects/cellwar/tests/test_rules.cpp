/*
 * test_rules.cpp - the rules of CellWar, played through the keys.
 *
 * Every test boots the built game, lays out a position on the board,
 * selects a unit with Tab and moves it with the arrows and SPACE, then
 * reads the board back: the stages, the moves, the attacks, the factory,
 * the turn order, the fog, the end of the game.  A lone computer unit far
 * from the action keeps the computer's turn harmless; a second unit of the
 * human's keeps the human's turn open while the first is checked.  Every
 * test skips when the game is not built.
 */
#include "CellwarGame.hpp"

using cellwar::CellwarGame;
using cellwar::Unit;
using cellwar::Own;
using cellwar::Moved;
using cellwar::Edge;
using cellwar::index;
using ms0515::Key;

namespace {

const std::vector<std::string> kEmpty = {
    "....................",
    "....................",
    "....................",
    "....................",
    "....................",
    "....................",
    "....................",
    "....................",
    "....................",
    "....................",
};

/* A board from a few (x, y, letter) placements on the empty one. */
std::vector<std::string> place(std::initializer_list<std::tuple<int, int, char>> units)
{
    std::vector<std::string> rows = kEmpty;
    for (auto [x, y, c] : units) rows[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
    return rows;
}

}  // namespace

TEST_CASE("cellwar: a new game - a fortification 2 each, the human to move, the fog around it")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_start");
    CHECK(g.cell(4, 4) == (Unit | 2));
    CHECK(g.cell(15, 5) == (Unit | Own | 2));
    CHECK(g.count(false) == 1);
    CHECK(g.count(true) == 1);
    CHECK(g.peek16("TURN") == 0);
    CHECK(g.peek16("ROUND") == 1);
    CHECK(g.peek16("GSTATE") == 0);
    CHECK(g.sel() == index(4, 4));
    CHECK(g.peek8("BOARD", cellwar::Cell0 - 1) == Edge);       // the frame round the map
    CHECK(g.seen(6, 6));                                        // radius 2 for a fortification
    CHECK(g.seen(2, 2));
    CHECK_FALSE(g.seen(7, 4));
    CHECK_FALSE(g.seen(1, 4));
    CHECK_FALSE(g.seen(15, 5));                                 // the enemy is in the fog
    CHECK(g.vramNonzero() > 1000);                              // something is drawn
}

TEST_CASE("cellwar: standing raises the stage, and a factory that stands stays a factory")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_stand");
    g.setBoard(place({{2, 2, 'I'}, {2, 5, 'A'}, {2, 8, 'B'}, {6, 2, 'F'}, {18, 8, 'i'}}));
    g.selectFirst();
    REQUIRE(g.sel() == index(2, 2));
    g.move(0, 0);
    CHECK(g.cell(2, 2) == (Unit | Moved | 1));                  // infantry -> fortification 1
    CHECK(g.sel() == index(2, 5));                              // the next unmoved, by itself
    g.move(0, 0);
    CHECK(g.cell(2, 5) == (Unit | Moved | 2));                  // 1 -> 2
    g.move(0, 0);
    CHECK(g.cell(2, 8) == (Unit | Moved | 3));                  // 2 -> factory
    CHECK(g.sel() == index(6, 2));                              // then the factories
    CHECK(g.peek16("PHASE") == 1);
    g.move(0, 0, 300);                                           // the last move: the round turns
    CHECK(g.peek16("ROUND") == 2);
    CHECK(g.peek16("TURN") == 0);
    CHECK(g.cell(6, 2) == (Unit | 3));                          // a factory still, unmoved again
    CHECK(g.cell(2, 8) == (Unit | 3));
}

TEST_CASE("cellwar: a factory makes an infantry and drops to fortification 1")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_produce");
    g.setBoard(place({{5, 5, 'F'}, {10, 8, 'F'}, {15, 9, 'F'}, {18, 0, 'i'}}));
    g.selectFirst();
    REQUIRE(g.sel() == index(5, 5));                            // no combat units: the factories
    g.move(1, 0);
    CHECK(g.cell(6, 5) == (Unit | Moved));                      // the new infantry, moved already
    CHECK(g.cell(5, 5) == (Unit | Moved | 1));                  // the factory spent
    CHECK(g.sel() == index(10, 8));
    g.move(-1, -1);
    CHECK(g.cell(9, 7) == (Unit | Moved));
    CHECK(g.cell(10, 8) == (Unit | Moved | 1));
}

TEST_CASE("cellwar: a factory cannot build on a unit or off the map, and stands when boxed in")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_boxed");
    g.setBoard(place({{0, 0, 'F'}, {1, 0, 'I'}, {0, 1, 'I'}, {1, 1, 'i'}, {12, 2, 'F'}, {18, 9, 'i'}}));
    g.selectFirst();
    REQUIRE(g.sel() == index(1, 0));
    g.move(0, 0);                                               // the two infantry stand
    g.move(0, 0);
    REQUIRE(g.sel() == index(0, 0));                            // the boxed factory
    g.move(1, 0);                                               // its own unit: no
    CHECK(g.sel() == index(0, 0));
    CHECK(g.cell(1, 0) == (Unit | Moved | 1));
    g.move(-1, 0);                                              // off the map: no
    CHECK(g.sel() == index(0, 0));
    g.move(1, 1);                                               // the enemy: a factory does not attack
    CHECK(g.sel() == index(0, 0));
    CHECK(g.cell(1, 1) == (Unit | Own));
    CHECK(g.cell(0, 0) == (Unit | 3));
    g.move(0, 0);                                               // it stands
    CHECK(g.cell(0, 0) == (Unit | Moved | 3));
    CHECK(g.sel() == index(12, 2));
}

TEST_CASE("cellwar: an infantry moves to an empty cell, not onto its own or off the map")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_move");
    g.setBoard(place({{0, 0, 'I'}, {5, 5, 'I'}, {6, 5, 'I'}, {18, 9, 'i'}}));
    g.selectFirst();
    REQUIRE(g.sel() == index(0, 0));
    g.move(-1, 0);                                              // off the map
    CHECK(g.sel() == index(0, 0));
    CHECK(g.cell(0, 0) == Unit);
    g.move(1, 1);
    CHECK(g.cell(1, 1) == (Unit | Moved));
    CHECK(g.cell(0, 0) == 0);
    REQUIRE(g.sel() == index(5, 5));
    g.move(1, 0);                                               // its own unit
    CHECK(g.sel() == index(5, 5));
    CHECK(g.cell(5, 5) == Unit);
    CHECK(g.cell(6, 5) == Unit);
    g.move(0, -1);
    CHECK(g.cell(5, 4) == (Unit | Moved));
    CHECK(g.cell(5, 5) == 0);
    CHECK(g.sel() == index(6, 5));
}

TEST_CASE("cellwar: an infantry kills an infantry or a factory and takes the cell")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_kill");
    g.setBoard(place({{5, 5, 'I'}, {6, 5, 'i'}, {5, 8, 'I'}, {4, 9, 'f'}, {0, 0, 'I'}, {0, 9, 'I'}, {18, 0, 'i'}}));
    g.selectFirst();
    g.move(0, 0);                                               // (0,0) stands, out of the way
    REQUIRE(g.sel() == index(5, 5));
    g.move(1, 0);
    CHECK(g.cell(6, 5) == (Unit | Moved));                      // the human's now
    CHECK(g.cell(5, 5) == 0);
    REQUIRE(g.sel() == index(5, 8));
    g.move(-1, 1);
    CHECK(g.cell(4, 9) == (Unit | Moved));
    CHECK(g.cell(5, 8) == 0);
    CHECK(g.count(true) == 1);
}

TEST_CASE("cellwar: a fortification takes two hits - the first attacker dies, it becomes an infantry")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_fort");
    g.setBoard(place({{5, 5, 'I'}, {6, 5, 'b'}, {7, 5, 'I'}, {0, 9, 'I'}, {18, 0, 'i'}}));
    g.selectFirst();
    REQUIRE(g.sel() == index(5, 5));
    g.move(1, 0);
    CHECK(g.cell(5, 5) == 0);                                   // the attacker is gone
    CHECK(g.cell(6, 5) == (Unit | Own));                        // the fortification is an infantry
    REQUIRE(g.sel() == index(7, 5));
    g.move(-1, 0);                                              // the second hit
    CHECK(g.cell(6, 5) == (Unit | Moved));
    CHECK(g.cell(7, 5) == 0);
    CHECK(g.count(true) == 1);
}

TEST_CASE("cellwar: a fortification moves as an infantry")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_fortmove");
    g.setBoard(place({{5, 5, 'B'}, {8, 5, 'A'}, {9, 5, 'a'}, {0, 9, 'I'}, {18, 0, 'i'}}));
    g.selectFirst();
    REQUIRE(g.sel() == index(5, 5));
    g.move(1, 0);
    CHECK(g.cell(6, 5) == (Unit | Moved));                      // an infantry again, moved
    CHECK(g.cell(5, 5) == 0);
    REQUIRE(g.sel() == index(8, 5));
    g.move(1, 0);                                               // one fortification on another
    CHECK(g.cell(8, 5) == 0);
    CHECK(g.cell(9, 5) == (Unit | Own));
}

TEST_CASE("cellwar: Tab cycles the unmoved combat units, then the factories")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_tab");
    g.setBoard(place({{2, 2, 'I'}, {6, 2, 'I'}, {4, 4, 'F'}, {18, 9, 'i'}}));
    g.selectFirst();
    REQUIRE(g.sel() == index(2, 2));
    g.tap(Key::Tab);
    CHECK(g.sel() == index(6, 2));
    g.tap(Key::Tab);
    CHECK(g.sel() == index(2, 2));                              // round again, the factory skipped
    g.move(0, 0);
    CHECK(g.sel() == index(6, 2));
    g.tap(Key::Tab);
    CHECK(g.sel() == index(6, 2));                              // the only one left
    g.move(0, 0);
    CHECK(g.sel() == index(4, 4));
    CHECK(g.peek16("PHASE") == 1);
}

TEST_CASE("cellwar: the cursor stays within a step of the unit")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_cursor");
    g.setBoard(place({{5, 5, 'I'}, {0, 9, 'I'}, {18, 0, 'i'}}));
    g.selectFirst();
    g.tap(Key::Right);
    g.tap(Key::Right);
    g.tap(Key::Down);
    CHECK(static_cast<int16_t>(g.peek16("CDX")) == 1);
    CHECK(static_cast<int16_t>(g.peek16("CDY")) == 1);
    g.tap(Key::Left);
    g.tap(Key::Left);
    g.tap(Key::Left);
    CHECK(static_cast<int16_t>(g.peek16("CDX")) == -1);
    g.tap(Key::Tab);                                            // another unit: the cursor on it
    CHECK(g.peek16("CDX") == 0);
    CHECK(g.peek16("CDY") == 0);
}

TEST_CASE("cellwar: the fog follows a move, the old view lasts the turn and no longer")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_fog");
    g.setBoard(place({{5, 5, 'I'}, {0, 9, 'I'}, {19, 0, 'i'}}));
    g.selectFirst();
    CHECK(g.seen(2, 2));                                        // the new game's view, still
    CHECK_FALSE(g.seen(7, 5));
    g.move(1, 0);                                               // to (6,5): radius 1 from there
    CHECK(g.seen(7, 5));
    CHECK(g.seen(7, 6));
    CHECK_FALSE(g.seen(8, 5));
    CHECK(g.seen(2, 2));                                        // the old view lasts the turn
    g.move(0, 0, 300);                                           // the turn ends, the round turns
    REQUIRE(g.peek16("ROUND") == 2);
    CHECK(g.seen(7, 5));
    CHECK_FALSE(g.seen(2, 2));                                  // and is gone with it
    CHECK(g.seen(1, 8));                                        // the other unit's own view
}

TEST_CASE("cellwar: the last enemy killed wins the game")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_win");
    g.setBoard(place({{5, 5, 'I'}, {6, 5, 'i'}}));
    g.selectFirst();
    g.move(1, 0);
    CHECK(g.peek16("GSTATE") == 1);
    CHECK(g.sel() == -1);
    CHECK(g.count(true) == 0);
}

TEST_CASE("cellwar: the last unit lost on a fortification loses the game")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_lose");
    g.setBoard(place({{5, 5, 'I'}, {6, 5, 'a'}}));
    g.selectFirst();
    g.move(1, 0);
    CHECK(g.peek16("GSTATE") == 2);
    CHECK(g.sel() == -1);
    CHECK(g.count(false) == 0);
}

TEST_CASE("cellwar: the computer moves every unit and the round comes back to the human")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_round");
    g.setBoard(place({{1, 1, 'I'}, {18, 8, 'i'}, {15, 5, 'f'}, {16, 1, 'b'}}));
    g.selectFirst();
    g.move(0, 0, 300);
    CHECK(g.peek16("ROUND") == 2);
    CHECK(g.peek16("TURN") == 0);
    CHECK(g.count(true) >= 3);                                  // nothing of theirs died
    int moved = 0;
    for (int y = 0; y < cellwar::Height; ++y)
        for (int x = 0; x < cellwar::Width; ++x)
            if ((g.cell(x, y) & (Unit | Own)) == (Unit | Own) && (g.cell(x, y) & Moved)) ++moved;
    CHECK(moved == g.count(true));                              // every one of them moved
    CHECK(g.cell(1, 1) == (Unit | 1));                          // ours unmoved again
    CHECK(g.sel() == index(1, 1));
}

TEST_CASE("cellwar: Q leaves for RT-11 with the machine as it was")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_quit");
    auto vector130 = [&g] { return static_cast<uint16_t>(g.board().mem.ram[0130] | (g.board().mem.ram[0131] << 8)); };
    const uint16_t high = static_cast<uint16_t>(g.sym("FONT") + 8 * 59);   // the program's end
    CHECK(vector130() >= 01000);                                // the game's keyboard handler
    CHECK(vector130() < high);
    CHECK((g.board().reg_c & 010) == 0);                        // 320x200
    CHECK(g.board().mem.dispatcher == 03177);                   // the frame interrupt on
    const uint16_t found = g.peek16("DSSAVE");                  // as RT-11 had it
    MESSAGE("the dispatcher RT-11 ran on: " << std::oct << found);
    CHECK((found & 0200) == 0);                                 // the window off
    g.tap(Key::Q, 60);
    CHECK(vector130() >= high);                                 // RT-11's handler again
    CHECK((g.board().reg_c & 010) != 0);                        // the console's 640x200
    CHECK(g.board().mem.dispatcher == found);                   // as found
}

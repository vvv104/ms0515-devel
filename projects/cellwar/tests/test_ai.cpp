/*
 * test_ai.cpp - the computer's play and the saved game.
 *
 * A position is laid out, the human's units stand, and what the computer
 * did with its turn is read off the board: the kills it takes, the
 * fortification it does not hit alone, the pair that takes one, the
 * factory that builds.  Then S and L: the game saved to the folder device
 * and brought back.  Every test skips when the game is not built.
 */
#include "CellwarGame.hpp"

using cellwar::CellwarGame;
using cellwar::Unit;
using cellwar::Own;
using cellwar::Moved;
using cellwar::index;
using ms0515::Key;

namespace {

std::vector<std::string> place(std::initializer_list<std::tuple<int, int, char>> units)
{
    std::vector<std::string> rows(cellwar::Height, std::string(cellwar::Width, '.'));
    for (auto [x, y, c] : units) rows[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
    return rows;
}

/* The human's units all stand; the computer's turn follows and the round
 * comes back. */
void humanStands(CellwarGame &g, int units)
{
    g.selectFirst();
    for (int i = 0; i < units - 1; ++i) g.move(0, 0);
    g.move(0, 0, 80);
    REQUIRE(g.peek16("ROUND") == 2);
}

}  // namespace

TEST_CASE("cellwar ai: an enemy factory next door is killed")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_ai_factory");
    g.setBoard(place({{5, 5, 'F'}, {0, 9, 'I'}, {6, 5, 'i'}}));
    humanStands(g, 2);
    CHECK((g.cell(5, 5) & (Unit | Own)) == (Unit | Own));       // the computer's now
    CHECK(g.cell(6, 5) == 0);
    CHECK(g.count(false) == 1);
}

TEST_CASE("cellwar ai: an enemy infantry next door is killed rather than waited for")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_ai_infantry");
    g.setBoard(place({{5, 5, 'A'}, {7, 5, 'I'}, {9, 5, 'i'}}));
    g.selectFirst();
    g.move(0, 0);                                               // the fortification stands
    g.move(1, 0, 80);                                           // the infantry steps next to theirs
    REQUIRE(g.peek16("ROUND") == 2);
    CHECK((g.cell(8, 5) & (Unit | Own)) == (Unit | Own));       // (8,5) taken
    CHECK(g.count(false) == 1);
}

TEST_CASE("cellwar ai: a fortification is not hit by one unit alone")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_ai_alone");
    g.setBoard(place({{5, 5, 'A'}, {0, 9, 'I'}, {6, 5, 'i'}}));
    humanStands(g, 2);
    CHECK((g.cell(5, 5) & (Unit | Own | 3)) == (Unit | 2));     // still a fortification, ours
    CHECK(g.count(true) == 1);                                  // and the computer's unit lives
}

TEST_CASE("cellwar ai: two units take a fortification together")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_ai_pair");
    g.setBoard(place({{5, 5, 'A'}, {0, 9, 'I'}, {6, 5, 'i'}, {6, 6, 'i'}}));
    humanStands(g, 2);
    CHECK((g.cell(5, 5) & (Unit | Own)) == (Unit | Own));       // taken
    CHECK(g.count(true) == 1);                                  // at the price of one
    CHECK(g.count(false) == 1);
}

TEST_CASE("cellwar ai: a factory of the computer's builds")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_ai_build");
    g.setBoard(place({{1, 1, 'I'}, {15, 5, 'f'}}));
    humanStands(g, 1);
    CHECK(g.cell(15, 5) == (Unit | Own | Moved | 1));           // spent to fortification 1
    CHECK(g.count(true) == 2);                                  // and the new infantry
}

TEST_CASE("cellwar ai: with nothing in view the army grows and still moves")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_ai_quiet");
    g.setBoard(place({{1, 1, 'I'}, {14, 4, 'i'}, {16, 6, 'i'}, {18, 8, 'i'}}));
    humanStands(g, 1);
    int moved = 0, forts = 0;
    for (int y = 0; y < cellwar::Height; ++y)
        for (int x = 0; x < cellwar::Width; ++x) {
            const uint8_t c = g.cell(x, y);
            if ((c & (Unit | Own)) != (Unit | Own)) continue;
            if (c & Moved) ++moved;
            if ((c & 3) == 1) ++forts;
        }
    CHECK(moved == 3);
    CHECK(forts >= 1);                                          // somebody started building
}

TEST_CASE("cellwar: S saves the game to DK:CELWAR.GAM and L brings it back")
{
    if (!cellwar::built()) { MESSAGE("CELLWAR not built - skipped"); return; }
    CellwarGame g("cellwar_save");
    g.setBoard(place({{2, 2, 'I'}, {7, 3, 'B'}, {12, 6, 'f'}, {18, 8, 'i'}}));
    g.selectFirst();
    g.move(1, 0);                                               // (2,2) -> (3,2), the turn open
    REQUIRE(g.sel() == index(7, 3));
    g.tap(Key::S, 120);
    CHECK((cellwar::fs::exists(g.bootDir() / "CELWAR.GAM") || cellwar::fs::exists(g.bootDir() / "celwar.gam")));
    g.setCell(3, 2, 0);                                         // the board spoilt
    g.setCell(7, 3, 0);
    g.poke16("SEL", 0xFFFF);
    g.tap(Key::L, 120);
    CHECK(g.cell(3, 2) == (Unit | Moved));
    CHECK(g.cell(7, 3) == (Unit | 2));
    CHECK(g.cell(12, 6) == (Unit | Own | 3));
    CHECK(g.sel() == index(7, 3));
    CHECK(g.peek16("ROUND") == 1);
    g.move(0, 0, 80);                                           // and it plays on: the last
    CHECK(g.peek16("ROUND") == 2);                              //   unmoved unit stood, the
    CHECK(g.cell(7, 3) == (Unit | 3));                          //   round turned
}

/*
 * test_crypt.cpp - the crawl, seen from outside.
 */

#include "Crawl.hpp"

#include <sstream>
#include <string>
#include <utility>

using namespace crypt;

namespace {

constexpr int kWarmUp = 200;     /* frames before the first key: the tables made, the screen up */

/* How many of the view's cells hold a lit pixel. */
int lit(Crawl &crawl)
{
    int n = 0;
    for (int y = kViewTop; y < kViewTop + kViewRows; ++y)
        for (int c = 0; c < kViewCells; ++c) n += crawl.cell(c, y)[0] != 0;
    return n;
}

/* The program says on its way out how many passes of its loop it made
 * in how many frames. */
std::pair<unsigned, unsigned> passesAndFrames(Crawl &crawl)
{
    crawl.tap('q');
    crawl.run(300);
    REQUIRE(crawl.machine.ended());
    unsigned passes = 0, frames = 0;
    const std::string printed = crawl.printed();
    MESSAGE(printed);
    std::istringstream line(printed);
    std::string word, in, passesWord;
    REQUIRE((line >> word >> passes >> passesWord >> in >> frames));
    REQUIRE(word == "CRYPT:");
    return {passes, frames};
}

}  // namespace

TEST_SUITE("CRYPT") {

TEST_CASE("the screen: the view with walls, the band, the party, the buttons") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    REQUIRE_FALSE(crawl.machine.ended());
    CHECK(crawl.machine.graphics());
    CHECK_FALSE(crawl.machine.emulator().isHires());
    CHECK(lit(crawl) > 200);                            /* walls and floor */
    CHECK(crawl.cell(1, 0)[1] == 0x0F);                 /* the band's text: white on blue */
    CHECK(crawl.cell(23, 45)[1] == 0x76);               /* a face: bright yellow, the head's own cell */
    CHECK(crawl.cell(22, 62)[1] == 0x42);               /* a full hit-point bar: bright red */
    CHECK(crawl.cell(2, 150)[1] == 0x4E);               /* a button's yellow label on bright blue */
}

TEST_CASE("a step forward slides the walls over several passes, a turn swings them") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    const auto still = crawl.view();                    /* the bricks: a step's parts show on them */
    crawl.run(10);
    CHECK(crawl.view() == still);                       /* nothing moves on its own */

    crawl.tap(ms0515::Key::Up);
    int changes = 0;
    auto before = crawl.view();
    for (int f = 0; f < 40; ++f) {
        crawl.run(1);
        auto now = crawl.view();
        if (now != before) ++changes;
        before = std::move(now);
    }
    CHECK(changes >= 4);                                /* shown in parts, not one jump */
    CHECK(crawl.view() != still);
    const auto stepped = crawl.view();

    crawl.tap(ms0515::Key::Right);
    changes = 0;
    before = crawl.view();
    for (int f = 0; f < 40; ++f) {
        crawl.run(1);
        auto now = crawl.view();
        if (now != before) ++changes;
        before = std::move(now);
    }
    CHECK(changes >= 4);
    CHECK(crawl.view() != stepped);
    /* The map: the party's arrow in the middle, a yellow cell among
     * white ones, with walls lit round it. */
    CHECK(crawl.cell(20, 168)[1] == 0x46);
    CHECK(crawl.cell(20, 168)[0] != 0);
    CHECK(crawl.cell(14, 140)[1] == 0x07);
    int walls = 0;
    for (int y = 140; y < 196; ++y) for (int c = 14; c < 26; ++c) walls += crawl.cell(c, y)[0] != 0;
    CHECK(walls > 40);
}

TEST_CASE("the walls do not let the party through") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    crawl.tap(ms0515::Key::Left);                       /* to face north: the wall of the ring */
    crawl.run(40);
    const auto facing = crawl.view();
    crawl.tap(ms0515::Key::Up);
    crawl.run(40);
    CHECK(crawl.view() == facing);
}

TEST_CASE("the pace: a pass every three frames, standing and moving") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    crawl.hold(ms0515::Key::Right, true);               /* turning round and round */
    crawl.run(120);
    crawl.hold(ms0515::Key::Right, false);
    crawl.run(20);
    const auto [passes, frames] = passesAndFrames(crawl);
    CHECK(frames >= 150);
    CHECK(passes * 3 <= frames);
    CHECK(passes * 30 >= frames * 9);
    CHECK(crawl.machine.emulator().isHires());
}

}

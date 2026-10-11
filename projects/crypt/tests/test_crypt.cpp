/*
 * test_crypt.cpp - the crawl, seen from outside.
 *
 * The party begins at the west end of a corridor running east, a wall
 * along its north side all the way: the view's left wall, half a cell
 * from the party's axis.  On the cylinder the view is drawn on, that
 * wall's top edge in pixel column x stands 120 sin|theta| rows above
 * the horizon, theta the column's angle - the same sum the host drew
 * it by, done again here from the geometry alone.
 */

#include "Crawl.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace crypt;

namespace {

constexpr int kWarmUp = 200;     /* frames before the first key: the data loaded, the screen up */
constexpr int kTurnPace = 5;     /* frames a pass of a turn takes (main.c TURN_PACE) */
constexpr int kStepPace = 6;     /* of a step (STEP_PACE) */
constexpr int kIdlePace = 2;
constexpr int kStepPasses = 5;   /* a step's phases (planes.py PHASES) */
constexpr int kTurnPasses = 8;

/* Frames run until the view has stood still for a dozen of them: a
 * move over, whatever its pace. */
void settle(Crawl &crawl)
{
    auto last = crawl.view();
    for (int still = 0, f = 0; still < 12 && f < 400; ++f) {
        crawl.run(1);
        auto now = crawl.view();
        still = now == last ? still + 1 : 0;
        last = std::move(now);
    }
}

/* How many of the view's cells hold a lit pixel. */
int lit(Crawl &crawl)
{
    int n = 0;
    for (int y = kViewTop; y < kViewTop + kViewRows; ++y)
        for (int c = 0; c < kViewCells; ++c) n += crawl.cell(c, y)[0] != 0;
    return n;
}

/* Whether the pixel at view column x, window row r is not black. */
bool coloured(Crawl &crawl, int x, int r)
{
    const uint8_t *cell = crawl.cell(x >> 3, kViewTop + r);
    const bool ink = cell[0] & (0x80 >> (x & 7));
    const int colour = ink ? cell[1] & 7 : (cell[1] >> 3) & 7;
    return colour != 0;
}

/* The colour of the pixel at view column x, window row r: 0 black,
 * 2 red, 4 green. */
int colourAt(Crawl &crawl, int x, int r)
{
    const uint8_t *cell = crawl.cell(x >> 3, kViewTop + r);
    const bool ink = cell[0] & (0x80 >> (x & 7));
    return ink ? cell[1] & 7 : (cell[1] >> 3) & 7;
}

/* The first window row of the pixel column that is not black, or -1. */
int firstColoured(Crawl &crawl, int x)
{
    for (int r = 0; r < kViewRows; ++r)
        if (coloured(crawl, x, r)) return r;
    return -1;
}

/* Where the left wall's top edge should be in view pixel column x: a
 * wall half a cell from the axis, drawn to 24 cells deep. */
int wallTop(int x)
{
    const double pi = 3.14159265358979323846;
    const double theta = (x + 40 + 0.5 - 128) * (pi / 2) / 256;      /* the view begins at sector pixel 40 */
    if (theta >= 0) return -1;
    if (0.5 / std::tan(-theta) > 24) return -1;
    const double half = 120 * std::sin(-theta);
    const int row = static_cast<int>(std::floor(59.5 - half)) + 1;
    return row < 0 ? 0 : row;
}

/* The walls' outline by cells: for each cell column of the view, the
 * first window row whose cell has a colour, ink or paper, and the last.
 * Blind to the texture - its black lines move as the party steps -
 * and so what a step must leave as it is. */
std::vector<int> silhouette(Crawl &crawl)
{
    std::vector<int> rows;
    for (int c = 0; c < kViewCells; ++c) {
        int first = -1, last = -1;
        for (int r = 0; r < kViewRows; ++r)
            if (crawl.cell(c, kViewTop + r)[1] & 0x3F) { if (first < 0) first = r; last = r; }
        rows.push_back(first);
        rows.push_back(last);
    }
    return rows;
}

/* The program says on its way out how many passes of its loop it made
 * in how many frames, and how many of them drew the view in how many. */
struct Tally { unsigned passes, frames, drawn, drawnFrames; };

Tally tally(Crawl &crawl)
{
    crawl.tap('q');
    crawl.run(300);
    REQUIRE(crawl.machine.ended());
    Tally t{};
    const std::string printed = crawl.printed();
    MESSAGE(printed);
    std::istringstream line(printed);
    std::string word, in, passesWord, comma, drawnWord;
    REQUIRE((line >> word >> t.passes >> passesWord >> in >> t.frames >> comma >> t.drawn >> drawnWord >> in >> t.drawnFrames));
    REQUIRE(word == "CRYPT:");
    return t;
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
    CHECK(lit(crawl) > 100);                            /* the walls' lines and outlines, the floor's */
    CHECK(crawl.cell(1, 0)[1] == 0x0F);                 /* the band's text: white on blue */
    CHECK(crawl.cell(23, 45)[1] == 0x76);               /* a face: bright yellow, the head's own cell */
    CHECK(crawl.cell(22, 62)[1] == 0x42);               /* a full hit-point bar: bright red */
    CHECK(crawl.cell(2, 150)[1] == 0x4E);               /* a button's yellow label on bright blue */
}

TEST_CASE("the left wall's top edge runs straight to the horizon, pixel by pixel") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    REQUIRE_FALSE(crawl.machine.ended());
    /* The edge is where the sum says in every column but those where
     * the texture's own black lines - the mortar between bricks, a
     * pixel in thirty-two - run down from it; the wall never stands
     * above the edge. */
    int exact = 0;
    std::ostringstream seen;
    for (int x = 0; x < 80; ++x) {
        const int expected = wallTop(x), got = firstColoured(crawl, x);
        seen << x << ":" << got << "/" << expected << " ";
        CAPTURE(x); CAPTURE(expected); CAPTURE(got);
        REQUIRE(expected >= 0);
        CHECK(got >= expected);
        exact += got == expected;
    }
    MESSAGE("column: the first row seen / expected - " << seen.str());
    CHECK(exact >= 72);
}

TEST_CASE("the bricks: three rows of mortar at the quarters of the wall, joints between, the floor's edges") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    REQUIRE_FALSE(crawl.machine.ended());
    /* The middle row of mortar lies on the horizon, row 60 of the
     * window, along the whole left wall: black, with the wall's red
     * right under it - but where a joint runs down. */
    int black = 0, redBelow = 0;
    for (int x = 0; x < 80; ++x) {
        black += !coloured(crawl, x, 60);
        redBelow += coloured(crawl, x, 61);
    }
    CHECK(black == 80);
    CHECK(redBelow >= 60);
    /* The rows of mortar above and below, at the quarters of the
     * wall's height in that column - 120 sin(theta) rows above and
     * below the horizon - and the red of the bricks between. */
    const auto half = [](int x) {
        return static_cast<int>(std::lround(120 * std::sin(-(x + 40 + 0.5 - 128) * 3.14159265358979323846 / 2 / 256)));
    };
    for (int x : {8, 24, 40}) {
        CAPTURE(x);
        CHECK_FALSE(coloured(crawl, x, 60 - half(x) / 2));
        CHECK_FALSE(coloured(crawl, x, 60 + half(x) / 2));
        CHECK(coloured(crawl, x, 60 - half(x) / 4));
        CHECK(coloured(crawl, x, 60 + half(x) / 4));
    }
    /* Joints: black pixels in the brick rows of the near wall, more
     * than the mortar alone; and at least one full black row across a
     * floor cell - the edge of a cell of the floor. */
    int joints = 0;
    for (int x = 0; x < 48; ++x) {                     /* a joint: black down the second brick row */
        bool down = true;
        for (int r = 44; r <= 56 && down; ++r) down = !coloured(crawl, x, r);
        joints += down;
    }
    CHECK(joints >= 1);
    CHECK(joints <= 6);
    int edges = 0;
    for (int r = 100; r < kViewRows; ++r) edges += crawl.cell(10, kViewTop + r)[0] == 0xFF;
    CHECK(edges >= 1);
}

TEST_CASE("the wall's foot meets the floor: no black under it but the floor's own lines") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    REQUIRE_FALSE(crawl.machine.ended());
    /* Under the left wall's foot in pixel column x - its top edge's
     * row from the bottom - every pixel is the floor's green, or black
     * across the whole cell: the edge of a floor cell.  The wall's
     * edge crosses a cell in a slant, and the cell's rows that are part
     * wall and part floor have no black to spare for triangles. */
    int checked = 0;
    for (int x = 3; x < 48; ++x) {
        const int top = wallTop(x);
        if (top < 1) continue;
        for (int r = kViewRows - top + 1; r < kViewRows; ++r) {
            const int colour = colourAt(crawl, x, r);
            const bool edge = crawl.cell(x >> 3, kViewTop + r)[0] == 0xFF;
            CAPTURE(x); CAPTURE(r); CAPTURE(colour);
            CHECK((colour == 4 || edge));
            ++checked;
        }
    }
    CHECK(checked > 200);
}

TEST_CASE("two steps on and two back leave the picture as it was drawn afresh") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    const auto start = crawl.view();
    /* The walls ahead grow as the party comes and shrink as it goes
     * back, the layer's columns brought up to date row by row; back
     * on the first cell the picture is the one composed whole. */
    for (int i = 0; i < 2; ++i) { crawl.tap(ms0515::Key::Up); settle(crawl); }
    CHECK(crawl.view() != start);
    crawl.png("twoOn");
    for (int i = 0; i < 2; ++i) { crawl.tap(ms0515::Key::Down); settle(crawl); }
    crawl.png("twoBack");
    {
        const auto back = crawl.view();
        std::ostringstream where;
        int n = 0;
        for (std::size_t i = 0; i < back.size(); i += 2)
            if ((back[i] != start[i] || back[i + 1] != start[i + 1]) && n++ < 12) {
                char cell[40];
                std::snprintf(cell, sizeof cell, " c%dr%d:%02X/%02X>%02X/%02X", static_cast<int>((i / 2) % kViewCells),
                              static_cast<int>((i / 2) / kViewCells), start[i], start[i + 1], back[i], back[i + 1]);
                where << cell;
            }
        MESSAGE("back on the first cell, cells differing from the start: " << n << where.str());
    }
    CHECK(crawl.view() == start);
}

TEST_CASE("up to the wall ahead: its corners lined, its mortar following, a turn there and back changing nothing") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    for (int i = 0; i < 8; ++i) { crawl.tap(ms0515::Key::Up); settle(crawl); }
    crawl.png("wallTwo");
    /* Two cells from the wall ahead it is 80 rows tall in the middle
     * column, with mortar at the quarters.  On the left the corridor's
     * wall meets it in a corner, and a line stands down the corner at
     * view pixel 36, where the face across ends, with no joint of the
     * side wall beside it; on the right the corridor's wall ends a cell
     * before, and the wall across goes on flat past pixel 139: no
     * line there. */
    for (int r : {40, 60, 80}) CHECK_FALSE(coloured(crawl, 84, r));
    for (int r : {30, 50, 70, 90}) CHECK(coloured(crawl, 84, r));
    for (int r = 24; r <= 96; ++r) { CAPTURE(r); CHECK_FALSE(coloured(crawl, 36, r)); }
    for (int r = 24; r <= 40; ++r) {
        CAPTURE(r);
        CHECK(coloured(crawl, 35, r));
        CHECK(coloured(crawl, 38, r));
        CHECK(coloured(crawl, 139, r));
    }
    /* One cell from it the wall overflows the window: the top row and
     * the horizon are mortar, nothing else in the middle column is
     * black - the mortar of the step before has not stayed behind. */
    crawl.tap(ms0515::Key::Up); settle(crawl);
    crawl.png("wallOne");
    CHECK_FALSE(coloured(crawl, 84, 0));
    CHECK_FALSE(coloured(crawl, 84, 60));
    for (int r = 1; r < kViewRows; ++r) {
        if (r == 60) continue;
        CAPTURE(r);
        CHECK(coloured(crawl, 84, r));
    }
    /* A turn away and back: the picture as it was. */
    const auto at = crawl.view();
    crawl.tap(ms0515::Key::Left); settle(crawl);
    crawl.tap(ms0515::Key::Right); settle(crawl);
    crawl.png("wallBack");
    CHECK(crawl.view() == at);
}

TEST_CASE("a step forward runs the joints along the walls and moves nothing else") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    crawl.png("start");
    const auto still = crawl.view();
    const auto outline = silhouette(crawl);
    crawl.run(10);
    CHECK(crawl.view() == still);                       /* nothing moves on its own */

    /* The frames of the step, each kept when the screen stood still
     * over a frame: the walls' outline never moves in a step, however
     * the texture runs - but for the far end of the corridor, which
     * comes nearer, in the middle columns. */
    crawl.tap(ms0515::Key::Up);
    int changes = 0;
    auto before = crawl.view();
    for (int f = 0; f < kStepPasses * kStepPace + 6; ++f) {
        crawl.run(1);
        auto now = crawl.view();
        if (now == before) {
            const auto seen = silhouette(crawl);
            for (int c = 0; c < kViewCells; ++c) {
                if (c >= 9 && c <= 12) continue;
                CAPTURE(f); CAPTURE(c);
                CHECK(seen[2 * c] == outline[2 * c]);
                CHECK(seen[2 * c + 1] == outline[2 * c + 1]);
            }
        } else {
            ++changes;
            crawl.png("step" + std::to_string(f));
        }
        before = std::move(now);
    }
    CHECK(changes >= kStepPasses - 1);                  /* shown in parts, not one jump */
    CHECK(crawl.view() != still);                       /* a cell on: the end of the corridor nearer */
    CHECK(crawl.cell(20, 168)[1] == 0x46);              /* the map: the party's arrow, yellow */
    CHECK(crawl.cell(20, 168)[0] != 0);
}

TEST_CASE("a turn slides the picture four columns a pass; four of them come round to the start") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    const auto start = crawl.view();
    /* The frames of the turn, each kept when the screen stood still
     * over a frame (a pass painted, the next not begun): a pass's
     * picture is the one before moved four cells left, the four on the
     * right new. */
    crawl.tap(ms0515::Key::Right);
    std::vector<std::vector<uint8_t>> frames;
    auto last = crawl.view();
    for (int f = 0; f < kTurnPasses * kTurnPace + 8; ++f) {
        crawl.run(1);
        auto now = crawl.view();
        if (now == last && (frames.empty() || frames.back() != now)) {
            frames.push_back(now);
            crawl.png("turn" + std::to_string(frames.size()));
        }
        last = std::move(now);
    }
    MESSAGE("stable frames of the turn: " << frames.size());
    CHECK(frames.size() >= 4);
    /* Each kept frame is the one before moved left by four cells a
     * pass - by eight or twelve when a pass went by between them. */
    const auto slidBy = [&](const std::vector<uint8_t> &now, const std::vector<uint8_t> &before, int cells) {
        for (int y = 0; y < kViewRows; ++y)
            for (int c = 0; c < kViewCells - cells; ++c)
                if (now[(y * kViewCells + c) * 2] != before[(y * kViewCells + c + cells) * 2] ||
                    now[(y * kViewCells + c) * 2 + 1] != before[(y * kViewCells + c + cells) * 2 + 1])
                    return false;
        return true;
    };
    /* How many cells of the overlap differ when the frame before is
     * moved by `cells`: 0 for a clean slide. */
    const auto differing = [&](const std::vector<uint8_t> &now, const std::vector<uint8_t> &before, int cells) {
        int n = 0;
        for (int y = 0; y < kViewRows; ++y)
            for (int c = 0; c < kViewCells - cells; ++c)
                n += now[(y * kViewCells + c) * 2] != before[(y * kViewCells + c + cells) * 2] ||
                     now[(y * kViewCells + c) * 2 + 1] != before[(y * kViewCells + c + cells) * 2 + 1];
        return n;
    };
    int slides = 0;
    for (std::size_t i = 1; i < frames.size(); ++i) {
        CAPTURE(i);
        bool slid = false;
        for (int k = 4; k <= 20 && !slid; k += 4) slid = slidBy(frames[i], frames[i - 1], k);
        if (!slid) {
            std::ostringstream what;
            for (int k = 0; k <= 20; k += 4) what << " by " << k << ": " << differing(frames[i], frames[i - 1], k);
            MESSAGE("frame " << i << " against the one before, cells differing" << what.str());
        }
        CHECK(slid);
        slides += slid;
    }
    CHECK(slides + 1 == static_cast<int>(frames.size()));
    settle(crawl);
    CHECK(crawl.view() != start);
    const auto turned = crawl.view();

    crawl.tap(ms0515::Key::Left);                       /* and back: the start again */
    settle(crawl);
    {
        const auto back = crawl.view();
        std::ostringstream where;
        int n = 0;
        for (std::size_t i = 0; i < back.size(); i += 2)
            if ((back[i] != start[i] || back[i + 1] != start[i + 1]) && n++ < 12) {
                char cell[40];
                std::snprintf(cell, sizeof cell, " c%dr%d:%02X/%02X>%02X/%02X", static_cast<int>((i / 2) % kViewCells),
                              static_cast<int>((i / 2) / kViewCells), start[i], start[i + 1], back[i], back[i + 1]);
                where << cell;
            }
        MESSAGE("back from the turn, cells differing from the start: " << n << where.str());
    }
    CHECK(crawl.view() == start);
    crawl.tap(ms0515::Key::Right);
    settle(crawl);
    CHECK(crawl.view() == turned);
    for (int t = 0; t < 3; ++t) {                       /* three more quarters: round to the start */
        crawl.tap(ms0515::Key::Right);
        settle(crawl);
    }
    MESSAGE("round the four quarters, cells differing from the start: " << differing(crawl.view(), start, 0));
    CHECK(crawl.view() == start);
}

TEST_CASE("the walls do not let the party through") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    crawl.tap(ms0515::Key::Left);                       /* to face north: the wall of the ring */
    settle(crawl);
    const auto facing = crawl.view();
    crawl.tap(ms0515::Key::Up);
    settle(crawl);
    CHECK(crawl.view() == facing);
}

/* The pace held with the key down for `frames`: no pass shorter than
 * the standing pace, and the passes that draw the view inside the
 * move's own pace, taken together - what one costs is told. */
void pace(ms0515::Key key, int frames, int movePace)
{
    Crawl crawl;
    crawl.run(kWarmUp);
    crawl.hold(key, true);
    crawl.run(frames);
    crawl.hold(key, false);
    crawl.run(20);
    const Tally t = tally(crawl);
    MESSAGE("a drawing pass: " << (t.drawn ? 10 * t.drawnFrames / t.drawn : 0) << " tenths of a frame, the pace " << movePace);
    CHECK(t.frames >= static_cast<unsigned>(frames));
    CHECK(t.passes * kIdlePace <= t.frames);
    CHECK(t.drawn >= 20);
    CHECK(t.drawnFrames < t.drawn * movePace);
    CHECK(crawl.machine.emulator().isHires());
}

TEST_CASE("the pace: a pass of a turn in five frames, turning round and round") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    pace(ms0515::Key::Right, 150, kTurnPace);
}

TEST_CASE("the pace: a pass of a step in five frames, walking the corridor") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    pace(ms0515::Key::Up, 150, kStepPace);
}

}

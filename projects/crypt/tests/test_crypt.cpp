/*
 * test_crypt.cpp - the crawl, seen from outside.
 *
 * The party begins at the west end of a corridor running east, a wall
 * along its north side all the way: the view's left wall, half a cell
 * from the party's axis.  On the cylinder the view is drawn on, that
 * wall stands 120 sin|theta| rows above and below the horizon in pixel
 * column x, theta the column's angle - the same sum the host made the
 * tables by, done again here from the geometry alone.
 */

#include "Crawl.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace crypt;

namespace {

constexpr int kWarmUp = 200;     /* frames before the first key: the screen up */
constexpr int kTurnPace = 2;     /* frames a pass of a turn takes (main.c TURN_PACE) */
constexpr int kStepPace = 4;     /* of a step (STEP_PACE) */
constexpr int kIdlePace = 2;
constexpr int kStepPasses = 5;   /* a step's phases (geometry.py PHASES) */
constexpr int kTurnPasses = 8;
constexpr uint8_t kPaper = 0x38; /* the view's one colour: white, the lines black */

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

/* Whether the pixel at view column x, window row r is a line. */
bool black(Crawl &crawl, int x, int r)
{
    return crawl.cell(x >> 3, kViewTop + r)[0] & (0x80 >> (x & 7));
}

constexpr double kPi = 3.14159265358979323846;

/* The angle of view pixel column x, radians, 0 straight ahead,
 * negative to the left; the view begins at sector pixel 40. */
double angleOf(int x) { return (x + 40 + 0.5 - 128) * (kPi / 2) / 256; }

/* Half the height of the left wall in view pixel column x, as the
 * tables have it: 120 sin|theta| rounded, theta the column's angle;
 * 0 where the ray meets it past the four rows of cells looked at, 4.5
 * cells ahead at the start - the dark there. */
int wallHalf(int x)
{
    const double theta = angleOf(x);
    if (theta >= 0 || 0.5 / std::tan(-theta) > 4.5) return 0;
    const int half = static_cast<int>(std::nearbyint(120 * std::sin(-theta)));
    return half < 1 ? 1 : half;
}

/* Whether a seam of the left wall - a cell's corner, n + 0.5 cells
 * ahead and half a cell aside - falls in view pixel column x, as
 * geometry.py puts the corners. */
bool seamAt(int x)
{
    for (int n = 0; n <= 16; ++n) {
        const int px = static_cast<int>(std::lround(std::atan2(0.5, n + 0.5) * 256 / (kPi / 2)));
        if (128 - std::min(px, 128) - 40 == x) return true;
    }
    return false;
}

/* The window row where the far edge of row n's cells crosses the
 * floor in view column c at the start, as geometry.py has it. */
int crossRow(int c, int n)
{
    const double r = (n + 0.5) / std::cos(angleOf(c * 8 + 3));
    return 60 + static_cast<int>(std::lround(60 / r));
}

/* Where two views differ, for a message. */
std::string differences(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    std::ostringstream where;
    int n = 0;
    for (std::size_t i = 0; i < a.size(); i += 2)
        if ((a[i] != b[i] || a[i + 1] != b[i + 1]) && n++ < 12) {
            char cell[40];
            std::snprintf(cell, sizeof cell, " c%dr%d:%02X/%02X>%02X/%02X", static_cast<int>((i / 2) % kViewCells),
                          static_cast<int>((i / 2) / kViewCells), a[i], a[i + 1], b[i], b[i + 1]);
            where << cell;
        }
    return std::to_string(n) + where.str();
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

TEST_CASE("the screen: the view's edges, the band, the party, the buttons") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    REQUIRE_FALSE(crawl.machine.ended());
    CHECK(crawl.machine.graphics());
    CHECK_FALSE(crawl.machine.emulator().isHires());
    CHECK(lit(crawl) > 100);                            /* the edges */
    for (int c = 0; c < kViewCells; c += 7)
        for (int r = 0; r < kViewRows; r += 17) { CAPTURE(c); CAPTURE(r); CHECK(crawl.cell(c, kViewTop + r)[1] == kPaper); }
    CHECK(crawl.cell(1, 0)[1] == 0x0F);                 /* the band's text: white on blue */
    CHECK(crawl.cell(23, 45)[1] == 0x78);               /* a face: its skin bright white under the nose's black */
    CHECK(crawl.cell(22, 62)[1] == 0x42);               /* a full hit-point bar: bright red */
    CHECK(crawl.cell(2, 150)[1] == 0x4E);               /* a button's yellow label on bright blue */
}

TEST_CASE("the left wall's top and foot run straight to the horizon, pixel by pixel") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    REQUIRE_FALSE(crawl.machine.ended());
    /* In every pixel column of the left wall its top edge is the pixel
     * at row 60 - half and its foot the pixel at 59 + half, and the
     * pixel between them on the horizon is clear - the wall is one
     * colour, only its edges drawn; its last pixel before the dark is
     * its end, a line. */
    int columns = 0;
    for (int x = 0; x < 80; ++x) {
        const int half = wallHalf(x);
        if (half == 0 || half > 60) continue;
        CAPTURE(x); CAPTURE(half);
        CHECK(black(crawl, x, 60 - half));
        CHECK(black(crawl, x, 59 + half));
        if (half > 2 && !seamAt(x) && wallHalf(x + 1) != 0) CHECK_FALSE(black(crawl, x, 60));
        ++columns;
    }
    CHECK(columns > 60);
}

TEST_CASE("every seam is a line: the cells' corners down the wall, the floor's and the ceiling's edges") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    REQUIRE_FALSE(crawl.machine.ended());
    /* The corners of the left wall's cells fall where geometry.py's
     * edge_px says: half a cell ahead and aside, 52 pixel columns left
     * of the sector's middle, view pixel 36; a cell and a half ahead,
     * 32 columns left, pixel 56.  A line stands down the wall there,
     * between its top and its foot; between the corners, none. */
    for (int x : {36, 56}) {
        const int half = wallHalf(x);
        for (int r = 61 - half; r < 59 + half; ++r) { CAPTURE(x); CAPTURE(r); CHECK(black(crawl, x, r)); }
    }
    for (int r = 40; r < 80; r += 5) { CAPTURE(r); CHECK_FALSE(black(crawl, 46, r)); }
    /* The floor's nearest edge across runs as a row through each
     * column, bowing a little from column to column as the cylinder
     * has it, right up to the wall's foot and not into the wall; the
     * ceiling's is its mirror. */
    for (int c = 0; c < 11; ++c) {
        const int row = crossRow(c, 1);
        for (int x = c * 8; x < c * 8 + 8; ++x) {
            const int half = wallHalf(x);
            CAPTURE(x); CAPTURE(row);
            if (half == 0 || 59 + half < row) {
                CHECK(black(crawl, x, row));
                CHECK(black(crawl, x, 119 - row));
            } else if (59 + half > row && !seamAt(x)) {
                CHECK_FALSE(black(crawl, x, row));
            }
        }
    }
}

TEST_CASE("a step forward runs the seams along the walls and moves no edge") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    crawl.png("start");
    const auto still = crawl.view();
    crawl.run(10);
    CHECK(crawl.view() == still);                       /* nothing moves on its own */

    /* The frames of the step, each kept when the screen stood still
     * over a frame: the walls' top and foot never move in a step,
     * however the seams run. */
    crawl.tap(ms0515::Key::Up);
    int changes = 0;
    auto before = crawl.view();
    for (int f = 0; f < kStepPasses * kStepPace + 6; ++f) {
        crawl.run(1);
        auto now = crawl.view();
        if (now == before) {
            for (int x = 4; x < 64; x += 3) {
                const int half = wallHalf(x);
                if (half == 0 || half > 60) continue;
                CAPTURE(f); CAPTURE(x);
                CHECK(black(crawl, x, 60 - half));
                CHECK(black(crawl, x, 59 + half));
            }
        } else {
            ++changes;
            crawl.png("step" + std::to_string(f));
        }
        before = std::move(now);
    }
    CHECK(changes >= kStepPasses - 1);                  /* shown in parts, not one jump */
    CHECK(crawl.view() == still);                       /* a cell on, the same four cells of corridor ahead */
    CHECK(crawl.cell(20, 168)[1] == 0x46);              /* the map: the party's arrow, yellow */
    CHECK(crawl.cell(20, 168)[0] != 0);
}

TEST_CASE("six steps on and six back leave the picture as it was") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    const auto start = crawl.view();
    for (int i = 0; i < 6; ++i) { crawl.tap(ms0515::Key::Up); settle(crawl); }
    CHECK(crawl.view() != start);                       /* the corridor's end come within four cells */
    crawl.png("sixOn");
    for (int i = 0; i < 6; ++i) { crawl.tap(ms0515::Key::Down); settle(crawl); }
    crawl.png("twoBack");
    MESSAGE("back on the first cell, cells differing from the start: " << differences(start, crawl.view()));
    CHECK(crawl.view() == start);
}

TEST_CASE("up to the wall ahead: a frame, its corners where the side walls end, and a turn there and back") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    for (int i = 0; i < 8; ++i) { crawl.tap(ms0515::Key::Up); settle(crawl); }
    crawl.png("wallTwo");
    /* Two cells from the wall ahead it is a frame: its top row and
     * its foot row across the middle columns, the foot the row above
     * the floor's edge it stands on, and its sides down view pixels 36
     * and 139, where the side walls' last cells end - the cell's near
     * corners, 52 pixel columns either side of the middle (geometry.py
     * edge_px). */
    for (int c = 5; c < 17; ++c) {
        const int half = crossRow(c, 1) - 60;
        for (int x = c * 8; x < c * 8 + 8; x += 3) {
            CAPTURE(x); CAPTURE(half);
            CHECK(black(crawl, x, 60 - half));
            CHECK(black(crawl, x, 59 + half));
            /* The floor's edge under the wall is the wall's foot, one
             * line, not a second one beside it; the ceiling's the
             * same. */
            for (int r = 60 + half; r < 63 + half; ++r) {
                CAPTURE(r);
                CHECK_FALSE(black(crawl, x, r));
                CHECK_FALSE(black(crawl, x, 119 - r));
            }
        }
    }
    /* Its left edge is the seam at pixel 36, where the left wall's last
     * cell ends; on the right the corridor opens, and the wall ahead
     * goes on past the opening as another cell's face: the seam between
     * the two at pixel 140, the first of the next face. */
    const int half = crossRow(4, 1) - 60;
    for (int r = 61 - half; r < 59 + half; ++r) {
        CAPTURE(r);
        CHECK(black(crawl, 36, r));
        CHECK(black(crawl, 140, r));
        CHECK_FALSE(black(crawl, 37, r));
        CHECK_FALSE(black(crawl, 139, r));
        CHECK_FALSE(black(crawl, 141, r));
    }
    /* A step on and a turn away and back: the picture as it was. */
    crawl.tap(ms0515::Key::Up); settle(crawl);
    crawl.png("wallOne");
    const auto at = crawl.view();
    crawl.tap(ms0515::Key::Left); settle(crawl);
    crawl.tap(ms0515::Key::Right); settle(crawl);
    crawl.png("wallBack");
    MESSAGE("back from the turn, cells differing: " << differences(at, crawl.view()));
    CHECK(crawl.view() == at);
}

TEST_CASE("past four rows of cells ahead the view is dark") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    /* Down the corridor nothing stands within four cells: from the far
     * edge of row 4's cells, 4.5 ahead, on the floor, to its mirror on
     * the ceiling, the middle of the view is black - no line drawn in
     * it, nothing past it looked at - and the floor beyond its foot is
     * the floor. */
    for (int c = 9; c < 13; ++c) {
        const int half = crossRow(c, 4) - 60;
        for (int x = c * 8; x < c * 8 + 8; x += 2) {
            CAPTURE(x); CAPTURE(half);
            for (int r = 60 - half; r < 60 + half; ++r) { CAPTURE(r); CHECK(black(crawl, x, r)); }
            CHECK_FALSE(black(crawl, x, 60 + half + 1));
            CHECK_FALSE(black(crawl, x, 59 - half - 1));
        }
    }
}

TEST_CASE("the far walls of the fourth row's cells are theirs: drawn, not dark") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    using K = ms0515::Key;
    const auto walk = [&](K turn, int steps) {
        if (turn != K::Up) { crawl.tap(turn); settle(crawl); }
        for (int i = 0; i < steps; ++i) { crawl.tap(K::Up); settle(crawl); }
    };
    walk(K::Up, 8);             /* (9, 1) */
    walk(K::Right, 6);          /* south to (9, 7) */
    walk(K::Right, 2);          /* west to (7, 7) */
    walk(K::Left, 2);           /* south to (7, 9) */
    walk(K::Right, 6);          /* west to (1, 9) */
    walk(K::Left, 6);           /* south to (1, 15) */
    walk(K::Left, 9);           /* east to (10, 15) */
    walk(K::Right, 2);          /* south to (10, 17) */
    walk(K::Right, 5);          /* west to (5, 17) */
    crawl.png("farWalls");
    /* At (5, 17) facing west four cells of floor lead to a wall five
     * cells on, its face on the far edge of the fourth row, with an
     * opening on the left in that row onto a corridor going south,
     * walls behind it too.  Every one of them is a far wall of a cell
     * of the fourth row, drawn: none of the view is dark - about the
     * horizon only the lines down the corners, never four black pixels
     * side by side. */
    for (int r = 50; r < 70; ++r) {
        int run = 0, longest = 0;
        for (int x = 0; x < kViewCells * 8; ++x) {
            run = black(crawl, x, r) ? run + 1 : 0;
            longest = run > longest ? run : longest;
        }
        CAPTURE(r);
        CHECK(longest < 4);
    }
}

TEST_CASE("a corner with a farther one straight behind it, and the floor's lines up to the wall they meet") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    for (const ms0515::Key k : {ms0515::Key::Up, ms0515::Key::Up, ms0515::Key::Up, ms0515::Key::Up, ms0515::Key::Up,
                                ms0515::Key::Up, ms0515::Key::Up, ms0515::Key::Up, ms0515::Key::Right, ms0515::Key::Up,
                                ms0515::Key::Up, ms0515::Key::Up, ms0515::Key::Left}) {
        crawl.tap(k);
        settle(crawl);
    }
    crawl.png("cornerAhead");
    /* At (9, 4) facing east the wall ahead is a cell's block two cells
     * on, the corridor opening on its right.  Its near right corner,
     * half a cell aside and a cell and a half ahead, falls in view
     * pixel 140, and so does the far end of the wall a cell and a half
     * aside four and a half ahead, straight behind: the block's edge
     * is its own, a line down its last pixel the whole of its height,
     * not the farther wall's short one. */
    const int half = crossRow(17, 1) - 60;
    for (int r = 61 - half; r < 59 + half; ++r) {
        CAPTURE(r);
        CHECK(black(crawl, 139, r));
    }
    /* Facing south, the corridor runs three cells to a wall, with an
     * opening either side a cell before it; the floor's line half a
     * cell to the left runs along the opening to the wall behind it,
     * and meets its foot at row 76 by the corner at pixel 65: its last
     * pixel 64, row 77; the ceiling's the same upside down. */
    crawl.tap(ms0515::Key::Right);
    settle(crawl);
    crawl.png("openings");
    CHECK(black(crawl, 64, 77));
    CHECK(black(crawl, 64, 42));
    CHECK(black(crawl, 65, 76));
}

TEST_CASE("the map: a cell a byte, the arrow yellow alone, the same arrow every way") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    /* The map's cells are a byte wide and six rows down, 12 across and
     * 9 down from row 142; the party's is the seventh across, the fifth
     * down - byte 20, rows 166..171.  A byte has one ink, so only the
     * party's cell is yellow: the cells either side keep the walls'
     * white whatever they are. */
    const auto arrow = [&] {
        std::array<uint8_t, 6> rows{};
        for (int i = 0; i < 6; ++i) rows[i] = crawl.cell(20, 166 + i)[0];
        return rows;
    };
    const auto flip = [](std::array<uint8_t, 6> a) { std::reverse(a.begin(), a.end()); return a; };
    const auto mirror = [](std::array<uint8_t, 6> a) {
        for (auto &b : a) {
            uint8_t m = 0;
            for (int i = 0; i < 8; ++i) if (b & (1 << i)) m |= static_cast<uint8_t>(0x80 >> i);
            b = m;
        }
        return a;
    };
    std::array<std::array<uint8_t, 6>, 4> way{};      /* east, south, west, north */
    for (int w = 0; w < 4; ++w) {
        CAPTURE(w);
        for (int i = 0; i < 6; ++i) {
            CHECK(crawl.cell(20, 166 + i)[1] == 0x46);
            CHECK(crawl.cell(19, 166 + i)[1] == 0x07);
            CHECK(crawl.cell(21, 166 + i)[1] == 0x07);
        }
        way[w] = arrow();
        crawl.tap(ms0515::Key::Right);
        settle(crawl);
    }
    crawl.png("mapArrow");
    CHECK(way[0] == flip(way[0]));                      /* east and west even top and bottom */
    CHECK(way[2] == mirror(way[0]));                    /* west east's mirror */
    CHECK(way[3] == mirror(way[3]));                    /* north and south even left and right */
    CHECK(way[1] == flip(way[3]));                      /* south north upside down */
    /* Each comes to a point the way it goes: east's last column and
     * north's top row hold the fewest pixels of the arrow. */
    int last = 0, first = 0;
    for (int i = 0; i < 6; ++i) last += way[0][i] & 1;
    for (int i = 0; i < 8; ++i) first += (way[3][0] >> i) & 1;
    CHECK(last == 2);
    CHECK(first == 2);
    CHECK(way[0][2] == 0xFF);                           /* the shaft along the cell */
    CHECK((way[3][5] & 0x18) == 0x18);
}

TEST_CASE("a turn slides the picture four columns a pass; four of them come round to the start") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    for (int i = 0; i < 2; ++i) { crawl.tap(ms0515::Key::Up); settle(crawl); }   /* a wall beside on the right, to turn to */
    const auto start = crawl.view();
    /* The frames of the turn, every one: a pass moves the picture four
     * cells left and draws the four on the right new, so whenever the
     * screen is looked at, its left part is the start's moved left by
     * four cells for each pass gone by - in all but the moment of the
     * move itself.  Over the turn every one of the first five passes is
     * seen so. */
    crawl.tap(ms0515::Key::Right);
    const auto slidBy = [&](const std::vector<uint8_t> &now, int cells) {
        for (int y = 0; y < kViewRows; ++y)
            for (int c = 0; c + cells < kViewCells; ++c)
                if (now[(y * kViewCells + c) * 2] != start[(y * kViewCells + c + cells) * 2] ||
                    now[(y * kViewCells + c) * 2 + 1] != start[(y * kViewCells + c + cells) * 2 + 1])
                    return false;
        return true;
    };
    int frames = 0, matched = 0;
    bool seen[6] = {};
    for (int f = 0; f < kTurnPasses * kTurnPace + 8; ++f) {
        crawl.run(1);
        const auto now = crawl.view();
        if (now == start) continue;                    /* not begun yet */
        ++frames;
        crawl.png("turn" + std::to_string(f));
        for (int p = 1; p <= 5; ++p)
            if (slidBy(now, 4 * p)) { seen[p] = true; ++matched; break; }
    }
    MESSAGE("frames of the turn: " << frames << ", the start slid in " << matched);
    for (int p = 1; p <= 5; ++p) { CAPTURE(p); CHECK(seen[p]); }
    settle(crawl);
    CHECK(crawl.view() != start);
    const auto turned = crawl.view();

    crawl.tap(ms0515::Key::Left);                       /* and back: the start again */
    settle(crawl);
    MESSAGE("back from the turn, cells differing from the start: " << differences(start, crawl.view()));
    CHECK(crawl.view() == start);
    crawl.tap(ms0515::Key::Right);
    settle(crawl);
    CHECK(crawl.view() == turned);
    for (int t = 0; t < 3; ++t) {                       /* three more quarters: round to the start */
        crawl.tap(ms0515::Key::Right);
        settle(crawl);
    }
    MESSAGE("round the four quarters, cells differing from the start: " << differences(start, crawl.view()));
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

TEST_CASE("the pace: a pass of a turn in two frames, turning round and round") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    pace(ms0515::Key::Right, 150, kTurnPace);
}

TEST_CASE("the pace: a pass of a step in four frames, walking the corridor") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    pace(ms0515::Key::Up, 150, kStepPace);
}

}

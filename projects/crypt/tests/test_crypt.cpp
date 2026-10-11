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
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace crypt;

namespace {

constexpr int kWarmUp = 200;     /* frames before the first key: the screen up */
constexpr int kTurnPace = 5;     /* frames a pass of a turn takes (main.c TURN_PACE) */
constexpr int kStepPace = 6;     /* of a step (STEP_PACE) */
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
 * 0 where the ray meets the corridor's end first, 9.5 cells ahead at
 * the start. */
int wallHalf(int x)
{
    const double theta = angleOf(x);
    if (theta >= 0 || 0.5 / std::tan(-theta) > 9.5) return 0;
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

/* The cosine of the middle of view column c in 8.8, as the tables
 * have it, at most 255 as the machine takes it. */
int cosineOf(int c)
{
    const int cosine = static_cast<int>(std::lround(256 * std::cos(angleOf(c * 8 + 3))));
    return cosine > 255 ? 255 : cosine;
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
    CHECK(crawl.cell(23, 45)[1] == 0x76);               /* a face: bright yellow, the head's own cell */
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
     * colour, only its edges drawn. */
    int columns = 0;
    for (int x = 0; x < 80; ++x) {
        const int half = wallHalf(x);
        if (half == 0 || half > 60) continue;
        CAPTURE(x); CAPTURE(half);
        CHECK(black(crawl, x, 60 - half));
        CHECK(black(crawl, x, 59 + half));
        if (half > 2 && !seamAt(x)) CHECK_FALSE(black(crawl, x, 60));
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
    CHECK(crawl.view() != still);                       /* a cell on: the end of the corridor nearer */
    CHECK(crawl.cell(20, 168)[1] == 0x46);              /* the map: the party's arrow, yellow */
    CHECK(crawl.cell(20, 168)[0] != 0);
}

TEST_CASE("two steps on and two back leave the picture as it was") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    Crawl crawl;
    crawl.run(kWarmUp);
    const auto start = crawl.view();
    for (int i = 0; i < 2; ++i) { crawl.tap(ms0515::Key::Up); settle(crawl); }
    CHECK(crawl.view() != start);
    crawl.png("twoOn");
    for (int i = 0; i < 2; ++i) { crawl.tap(ms0515::Key::Down); settle(crawl); }
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
     * its foot row across the middle columns, and its sides down view
     * pixels 36 and 139, where the side walls' last cells end - the
     * cell's near corners, 52 pixel columns either side of the middle
     * (geometry.py edge_px). */
    for (int c = 5; c < 17; ++c) {                     /* front_half 40 by each column's cosine */
        const int half = (40 * cosineOf(c)) >> 8;
        for (int x = c * 8; x < c * 8 + 8; x += 3) {
            CAPTURE(x); CAPTURE(half);
            CHECK(black(crawl, x, 60 - half));
            CHECK(black(crawl, x, 59 + half));
        }
    }
    /* Its left edge is the seam at pixel 36, where the left wall's last
     * cell ends; on the right the corridor opens, and the wall ahead
     * goes on past the opening as another cell's face: the seam between
     * the two at pixel 140, the first of the next face. */
    const int half = (40 * cosineOf(4)) >> 8;
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
    int slides = 0;
    for (std::size_t i = 1; i < frames.size(); ++i) {
        CAPTURE(i);
        bool slid = false;
        for (int k = 4; k <= 20 && !slid; k += 4) slid = slidBy(frames[i], frames[i - 1], k);
        CHECK(slid);
        slides += slid;
    }
    CHECK(slides + 1 == static_cast<int>(frames.size()));
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

TEST_CASE("the pace: a pass of a turn in five frames, turning round and round") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    pace(ms0515::Key::Right, 150, kTurnPace);
}

TEST_CASE("the pace: a pass of a step in five frames, walking the corridor") {
    if (!built()) { MESSAGE("CRYPT.SAV not built - skipped"); return; }
    pace(ms0515::Key::Up, 150, kStepPace);
}

}

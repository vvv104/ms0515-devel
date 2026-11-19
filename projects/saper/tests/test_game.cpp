/*
 * test_game.cpp - SAPER booted through RT-11 on a folder device and
 * checked from outside: the cursor on the board, an arrow tapped, an
 * arrow held.  Every test skips when the game is not built.
 */
#include "SaperGame.hpp"

#include <algorithm>
#include <array>
#include <vector>

using saper::SaperGame;
using ms0515::Key;

namespace {

std::vector<uint8_t> readBytes(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

/* The ROM's font, as the game takes it: KOI-8 040..0177 and 0300..0377,
 * each table found by the shape of one glyph; anything else is blank. */
struct RomFont {
    std::vector<uint8_t> rom = readBytes(std::string{ASSETS_DIR} + "/rom/ms0515-romb.rom");
    long base(const std::array<uint8_t, 8> &shape, int index) const
    {
        auto at = std::search(rom.begin(), rom.end(), shape.begin(), shape.end());
        REQUIRE(at != rom.end());
        return static_cast<long>(at - rom.begin()) - index * 8;
    }
    long main = base({0x00, 0x3C, 0x46, 0x4A, 0x52, 0x62, 0x3C, 0x00}, 16);
    long alt  = base({0x30, 0x78, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0x00}, 33);
    std::array<uint8_t, 8> glyph(int c) const
    {
        std::array<uint8_t, 8> g{};
        long at = c >= 040 && c < 0200 ? main + (c - 040) * 8
                : c >= 0300            ? alt + (c - 0300) * 8 : -1;
        if (at >= 0) std::copy_n(rom.begin() + at, 8, g.begin());
        return g;
    }
};

}  // namespace

TEST_CASE("saper: the board is up with the cursor on it")
{
    if (!saper::built()) { REQUIRE_FALSE(saper::named()); MESSAGE("SAPER not built - skipped"); return; }
    SaperGame g("saper_start");
    CHECK(g.word("CURX") >= g.word("XMIN"));
    CHECK(g.word("CURX") <= g.word("XMAX"));
}

TEST_CASE("saper: F1's help is the help text in the ROM's font, cell by cell")
{
    /* SAPER.HLP is K.HLP's 4096 bytes and then the 3680 of the noise that
     * decrypts it: character i is K[i] - N[i - 2], the first two as they are.
     * Page 1 is its first 23 lines of 80, a cell eight rows of a byte. */
    if (!saper::built()) { REQUIRE_FALSE(saper::named()); MESSAGE("SAPER not built - skipped"); return; }
    SaperGame g("saper_help");
    g.keyTap(Key::F1, 4);
    g.settle(50);
    const std::vector<uint8_t> hlp = readBytes(saper::hlpPath());
    REQUIRE(hlp.size() >= 4096 + 3680);
    const RomFont font;
    const uint8_t *v = g.vram();
    int wrong = 0, inked = 0;
    for (int i = 0; i < 23 * 80; ++i) {
        const int c = i < 2 ? hlp[i] : (hlp[i] - hlp[4096 + i - 2]) & 0xFF;
        const auto want = font.glyph(c);
        const int row = i / 80, col = i % 80;
        for (int y = 0; y < 8; ++y) {
            const uint8_t got = v[row * 640 + y * 80 + col];
            if (got != want[y]) ++wrong;
            if (got) ++inked;
        }
    }
    CHECK(inked > 1000);                           // a page of text, not a blank
    CHECK(wrong == 0);
}

TEST_CASE("saper: an arrow tapped moves the cursor one cell")
{
    if (!saper::built()) { REQUIRE_FALSE(saper::named()); MESSAGE("SAPER not built - skipped"); return; }
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
    if (!saper::built()) { REQUIRE_FALSE(saper::named()); MESSAGE("SAPER not built - skipped"); return; }
    SaperGame g("saper_hold");
    const int x0 = g.word("CURX");
    REQUIRE(g.word("XMAX") - x0 >= 3);
    g.emu.keyPress(Key::Right, true);
    g.settle(60);                                  // over a second of repeats
    g.emu.keyPress(Key::Right, false);
    g.settle(10);
    CHECK(g.word("CURX") == g.word("XMAX"));
}

/*
 * CellwarGame.hpp - the harness the CELLWAR tests run on.
 *
 * Boots RT-11 headlessly with the built game on a folder device (the
 * toolset's system template + CELWAR.SAV + a STARTS.COM that runs it),
 * answers the date prompts, and reads and writes the game's state where
 * the link map says it is: CELWAR.MAP names BOARD, VISH, VISC, TURN,
 * PHASE, ROUND, GSTATE, SEL, CDX, CDY, HCNT, CCNT.  The board is poked
 * between keys - the game reads it only while it handles one - so a test
 * lays out any position it likes, selects a unit with Tab and plays.
 * Every path has a default under the repo (overridable with
 * --cellwar-<name>=... options, see test_main.cpp), and the suite skips
 * itself when the game is not built.
 */
#pragma once

#include <doctest/doctest.h>
#include <ms0515/Emulator.hpp>
#include "EmulatorInternal.hpp"

extern "C" {
#include <ms0515/core/board.h>
#include <ms0515/core/memory.h>
}

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace cellwar {

namespace fs = std::filesystem;

/* The board as CELLWAR.MAC lays it out: 20 x 10 cells inside a frame of
 * two edge cells on every side, row-major, a byte a cell. */
constexpr int Width  = 20;
constexpr int Height = 10;
constexpr int PW     = 24;
constexpr int PH     = 14;
constexpr int Cells  = PW * PH;
constexpr int Cell0  = 2 * PW + 2;

constexpr uint8_t Unit  = 0200;   // a unit is here
constexpr uint8_t Own   = 0100;   // the computer's (clear: the human's)
constexpr uint8_t Moved = 010;    // has moved this round
constexpr uint8_t Stage = 3;      // 0 infantry, 1 and 2 fortification, 3 factory
constexpr uint8_t Edge  = 040;    // off the map

constexpr int index(int x, int y) { return Cell0 + y * PW + x; }

/* A cell's letter in a test's board picture: '.' empty; the human's
 * I A B F = infantry, fortification 1, fortification 2, factory; the
 * computer's i a b f. */
inline uint8_t unitOf(char c)
{
    switch (c) {
    case 'I': return Unit | 0;
    case 'A': return Unit | 1;
    case 'B': return Unit | 2;
    case 'F': return Unit | 3;
    case 'i': return Unit | Own | 0;
    case 'a': return Unit | Own | 1;
    case 'b': return Unit | Own | 2;
    case 'f': return Unit | Own | 3;
    default:  return 0;
    }
}

/* Harness options: --cellwar-<name>=<value> on the command line. */
inline std::map<std::string, std::string> &options()
{
    static std::map<std::string, std::string> opts;
    return opts;
}

inline std::string optOr(const char *name, const std::string &dflt)
{
    auto it = options().find(name);
    return it == options().end() || it->second.empty() ? dflt : it->second;
}

inline std::string savPath()   { return optOr("sav", std::string{CELLWAR_DIR} + "/CELWAR.SAV"); }
inline std::string mapPath()   { return optOr("map", std::string{CELLWAR_DIR} + "/CELWAR.MAP"); }
inline std::string systemDir() { return optOr("system", std::string{CELLWAR_SYSTEM_DIR}); }

/* The game is built?  Tests skip otherwise. */
inline bool built()
{
    return fs::exists(savPath()) && fs::exists(mapPath()) && fs::exists(systemDir());
}

inline std::string readText(const fs::path &p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

inline void writeFile(const fs::path &p, const void *data, size_t n)
{
    std::ofstream o(p, std::ios::binary);
    o.write(static_cast<const char *>(data), static_cast<std::streamsize>(n));
}

/* The link map's globals: "NAME  013376" pairs, octal. */
inline std::map<std::string, uint16_t> symbols()
{
    std::map<std::string, uint16_t> out;
    const std::string text = readText(mapPath());
    static const std::regex pair(R"(([A-Z][A-Z0-9$.]{0,5})\s+([0-7]{6}))");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pair); it != std::sregex_iterator(); ++it)
        out[(*it)[1].str()] = static_cast<uint16_t>(std::stoul((*it)[2].str(), nullptr, 8));
    return out;
}

class CellwarGame {
public:
    ms0515::Emulator emu;

    /* Stage the folder device under %TEMP%/<name>, mount it, boot into the
     * game: RT-11's prompts answered, "R CELWAR" run by STARTS.COM, the
     * board up by the end of `bootFrames`. */
    explicit CellwarGame(const char *name, int bootFrames = 1500)
    {
        syms_ = symbols();
        REQUIRE(syms_.count("BOARD"));
        stage(name);
        REQUIRE(emu.loadRomFile(std::string{ASSETS_DIR} + "/rom/ms0515-romb.rom"));
        emu.reset();
        REQUIRE(emu.mountDisk(0, (boot_ / "device.rtfs").string()));
        hookConsole();
        settle(bootFrames);
    }

    ms0515_board_t &board() { return ms0515::internal::board(emu); }

    uint16_t sym(const char *name) const
    {
        auto it = syms_.find(name);
        REQUIRE_MESSAGE(it != syms_.end(), name);
        return it->second;
    }
    /* The program lives in the primary banks below 70000: RAM as addressed. */
    uint8_t  peek8(const char *name, int off = 0)  { return board().mem.ram[sym(name) + off]; }
    uint16_t peek16(const char *name, int off = 0)
    {
        const uint8_t *p = &board().mem.ram[sym(name) + off];
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }
    void poke8(const char *name, int off, uint8_t v) { board().mem.ram[sym(name) + off] = v; }
    void poke16(const char *name, uint16_t v)
    {
        uint8_t *p = &board().mem.ram[sym(name)];
        p[0] = static_cast<uint8_t>(v & 0xFF);
        p[1] = static_cast<uint8_t>(v >> 8);
    }

    uint8_t cell(int x, int y)             { return peek8("BOARD", index(x, y)); }
    void    setCell(int x, int y, uint8_t v) { poke8("BOARD", index(x, y), v); }
    bool    seen(int x, int y)             { return peek8("VISH", index(x, y)) != 0; }
    bool    cpuSeen(int x, int y)          { return peek8("VISC", index(x, y)) != 0; }
    int16_t sel()                          { return static_cast<int16_t>(peek16("SEL")); }

    /* The whole board from a picture: Height rows of Width letters. */
    void setBoard(const std::vector<std::string> &rows)
    {
        REQUIRE(rows.size() == Height);
        for (int y = 0; y < Height; ++y) {
            REQUIRE(rows[static_cast<size_t>(y)].size() == Width);
            for (int x = 0; x < Width; ++x)
                setCell(x, y, unitOf(rows[static_cast<size_t>(y)][static_cast<size_t>(x)]));
        }
    }

    int count(bool computer)
    {
        int n = 0;
        for (int y = 0; y < Height; ++y)
            for (int x = 0; x < Width; ++x) {
                const uint8_t c = cell(x, y);
                if ((c & Unit) && ((c & Own) != 0) == computer) ++n;
            }
        return n;
    }

    /* One video frame, the keyboard's clock ticking (auto-repeat). */
    void step()
    {
        if (frame_ < 900 && frame_ % 30 == 0) offerCR_ = true;   // the date prompts
        nowMs_ += 20;
        emu.keyTick(nowMs_);
        (void)emu.stepFrame();
        ++frame_;
    }
    void settle(int n) { for (int i = 0; i < n; ++i) step(); }
    int frame() const { return frame_; }

    /* A key tapped: down for a few frames, then time for the game to act. */
    void tap(ms0515::Key k, int after = 8)
    {
        emu.keyPress(k, true);
        settle(4);
        emu.keyPress(k, false);
        settle(after);
    }

    /* After the board is poked: the selection scanned afresh from the
     * board's first cell, the combat units first.  Tab scans on from SEL
     * round the board, so SEL is set to the last cell (a -1 would mean no
     * unit to select from, and Tab would do nothing). */
    void selectFirst()
    {
        poke16("SEL", static_cast<uint16_t>(Cells - 1));
        poke16("PHASE", 0);
        tap(ms0515::Key::Tab);
    }

    /* A move of the selected unit: the cursor pushed dx, dy (each -1..1)
     * and SPACE. */
    void move(int dx, int dy, int after = 8)
    {
        using ms0515::Key;
        poke16("CDX", 0);                  // the cursor back on the unit: a refused
        poke16("CDY", 0);                  //   move leaves it where it was pushed
        if (dx < 0) tap(Key::Left);
        if (dx > 0) tap(Key::Right);
        if (dy < 0) tap(Key::Up);
        if (dy > 0) tap(Key::Down);
        tap(Key::Space, after);
    }

    /* What RT-11 printed on the console since the game started. */
    const std::string &console() const { return console_; }

    /* The folder device the game runs from: files the game makes land here. */
    const fs::path &bootDir() const { return boot_; }

    const uint8_t *vram() { return board_get_vram(&board()); }
    int vramNonzero()
    {
        const uint8_t *v = vram();
        int n = 0;
        for (int i = 0; i < MEM_VRAM_SIZE; ++i) if (v[i]) ++n;
        return n;
    }

private:
    fs::path boot_;
    bool offerCR_ = false;
    uint32_t nowMs_ = 0;
    int frame_ = 0;
    std::map<std::string, uint16_t> syms_;
    std::string console_;

    void stage(const char *name)
    {
        fs::path tmp = fs::temp_directory_path() / name;
        std::error_code ec;
        fs::remove_all(tmp, ec);
        boot_ = tmp / "boot";
        fs::create_directories(boot_, ec);
        fs::copy(systemDir(), boot_, fs::copy_options::recursive, ec);
        REQUIRE_FALSE(ec);
        fs::copy_file(savPath(), boot_ / "CELWAR.SAV", fs::copy_options::overwrite_existing, ec);
        const char starts[] = "R CELWAR\r\n";
        writeFile(boot_ / "STARTS.COM", starts, sizeof starts - 1);
    }

    /* RT-11's console is the serial port: answer the date/time prompts with
     * CRs, paced so they land on the prompts, not flood the input; keep
     * what it prints. */
    void hookConsole()
    {
        emu.setSerialCallbacks(
            [this](uint8_t &b) -> bool {
                if (offerCR_) { b = '\r'; offerCR_ = false; return true; }
                return false;
            },
            [this](uint8_t b) -> bool { console_ += static_cast<char>(b); return true; });
    }
};

}  // namespace cellwar

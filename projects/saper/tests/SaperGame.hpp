/*
 * SaperGame.hpp - the harness SAPER's tests run on.
 *
 * Boots RT-11 headlessly with the built game on a folder device (the
 * toolset's system template + SAPER.SAV, SAPER.HLP, SAPER.DAT + a
 * STARTS.COM that runs it), answers the date prompts, and reads the game's
 * state where the link map says it is: SAPER.MAP names CURX, CURY, XMIN,
 * XMAX.  Every path has a default under the repo (overridable with
 * --saper-<name>=... options, see test_main.cpp), and the suite skips
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

namespace saper {

namespace fs = std::filesystem;

/* Harness options: --saper-<name>=<value> on the command line. */
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

inline std::string gameFile(const char *ext) { return std::string{SAPER_DIR} + "/SAPER." + ext; }
inline std::string savPath()   { return optOr("sav", gameFile("SAV")); }
inline std::string hlpPath()   { return optOr("hlp", gameFile("HLP")); }
inline std::string datPath()   { return optOr("dat", gameFile("DAT")); }
inline std::string mapPath()   { return optOr("map", gameFile("MAP")); }
inline std::string systemDir() { return optOr("system", std::string{SAPER_SYSTEM_DIR}); }

/* The game is built?  Tests skip otherwise. */
inline bool built()
{
    return fs::exists(savPath()) && fs::exists(hlpPath()) && fs::exists(datPath())
        && fs::exists(mapPath()) && fs::exists(systemDir());
}

inline std::string readText(const fs::path &p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
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

class SaperGame {
public:
    ms0515::Emulator emu;

    /* Stage the folder device under %TEMP%/<name>, mount it, boot into the
     * game: RT-11's prompts answered, "R SAPER" run by STARTS.COM, the
     * board up by the end of `bootFrames`. */
    explicit SaperGame(const char *name, int bootFrames = 1500)
    {
        syms_ = symbols();
        REQUIRE(syms_.count("CURX"));
        stage(name);
        REQUIRE(emu.loadRomFile(std::string{ASSETS_DIR} + "/rom/ms0515-romb.rom"));
        emu.reset();
        REQUIRE(emu.mountDisk(0, (boot_ / "device.rtfs").string()));
        hookConsole();
        settle(bootFrames);
    }

    ms0515_board_t &board() { return ms0515::internal::board(emu); }

    /* A word of the game's, where the map puts it (below 30000: RAM as addressed). */
    int word(const char *name)
    {
        auto it = syms_.find(name);
        REQUIRE_MESSAGE(it != syms_.end(), name);
        const uint8_t *p = &board().mem.ram[it->second];
        return static_cast<int16_t>(p[0] | (p[1] << 8));
    }

    /* One video frame, the keyboard's clock ticking (auto-repeat). */
    void step()
    {
        if (frame_ < 900 && frame_ % 30 == 0) offerCR_ = true;   // the date prompts
        (void)emu.stepFrame();
        ++frame_;
    }
    void settle(int n) { for (int i = 0; i < n; ++i) step(); }

    void keyTap(ms0515::Key k, int holdFrames)
    {
        emu.keyPress(k, true);
        settle(holdFrames);
        emu.keyPress(k, false);
    }

private:
    fs::path boot_;
    bool offerCR_ = false;
    int frame_ = 0;
    std::map<std::string, uint16_t> syms_;

    void stage(const char *name)
    {
        fs::path tmp = fs::temp_directory_path() / name;
        std::error_code ec;
        fs::remove_all(tmp, ec);
        boot_ = tmp / "boot";
        fs::create_directories(boot_, ec);
        fs::copy(systemDir(), boot_, fs::copy_options::recursive, ec);
        REQUIRE_FALSE(ec);
        for (const auto &[from, to] : {std::pair{savPath(), "SAPER.SAV"}, {hlpPath(), "SAPER.HLP"},
                                       {datPath(), "SAPER.DAT"}}) {
            fs::copy_file(from, boot_ / to, fs::copy_options::overwrite_existing, ec);
            REQUIRE_FALSE(ec);
        }
        std::ofstream(boot_ / "STARTS.COM", std::ios::binary) << "R SAPER\r\n";
    }

    /* RT-11's console is the serial port: answer the date/time prompts with
     * CRs, paced so they land on the prompts, not flood the input. */
    void hookConsole()
    {
        emu.setSerialCallbacks(
            [this](uint8_t &b) -> bool {
                if (offerCR_) { b = '\r'; offerCR_ = false; return true; }
                return false;
            },
            [](uint8_t) -> bool { return true; });
    }
};

}  // namespace saper

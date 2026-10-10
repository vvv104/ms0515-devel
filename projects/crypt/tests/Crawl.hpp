/*
 * Crawl.hpp - CRYPT under test.
 *
 * CRYPT.SAV run the way `ms0515-run CRYPT` runs it - the machine of
 * ms0515-run, without a terminal or a window - the frames counted, the
 * keys pressed, the screen read out of video memory: 200 rows of 40
 * words, the pixel byte under the attribute byte.  The folder is the
 * project's build/sav unless --crypt-sav=<folder> names another, and
 * then a missing program is a failure; the suite skips itself when the
 * default folder has none.  With --crypt-png=<folder> the tests keep
 * what they saw as PNGs there, for looking at.
 */
#pragma once

#include <doctest/doctest.h>

#include "ConsoleText.hpp"
#include "Machine.hpp"
#include "EmulatorInternal.hpp"

#include <ms0515/Emulator.hpp>
#include <ms0515/Typist.hpp>
#include <ms0515/app/Screen.hpp>

extern "C" {
#include <ms0515/core/board.h>
}

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace crypt {

namespace fs = std::filesystem;

inline std::map<std::string, std::string> &options()
{
    static std::map<std::string, std::string> opts;
    return opts;
}

inline fs::path savDir()
{
    auto it = options().find("sav");
    return it == options().end() || it->second.empty() ? fs::path(CRYPT_SAV_DIR) : fs::path(it->second);
}

inline bool built()
{
    const bool there = fs::exists(savDir() / "CRYPT.SAV");
    if (!there && options().count("sav")) {
        const std::string missing = "CRYPT.SAV is not in " + savDir().string();
        FAIL(missing);
    }
    return there;
}

/* The view's window on the screen (view.h). */
constexpr int kViewCells = 22, kViewRows = 120, kViewTop = 16;

class Crawl {
public:
    ms0515::run::Machine machine;

    Crawl()
    {
        static int counter = 0;
        dir_ = fs::temp_directory_path() / ("ms0515_crypt_" + std::to_string(counter++));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        fs::copy_file(savDir() / "CRYPT.SAV", dir_ / "CRYPT.SAV");
        fs::copy_file(savDir() / "CRYPT.DAT", dir_ / "CRYPT.DAT");   /* the walls drawn on the host */
        REQUIRE(machine.start(dir_ / "CRYPT.SAV", {}));
    }
    ~Crawl()
    {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    Crawl(const Crawl &)            = delete;
    Crawl &operator=(const Crawl &) = delete;

    void run(int frames)
    {
        for (int f = 0; f < frames && !machine.ended(); ++f) {
            typist_.pump(machine.emulator());
            machine.step();
            printed_ += text_.convert(machine.takeOutput());
        }
    }
    void tap(ms0515::Key key) { typist_.type(key); }
    void tap(char c)          { typist_.type(static_cast<uint8_t>(c)); }
    /* A key held down: the keyboard repeats it, as a real one does. */
    void hold(ms0515::Key key, bool down) { machine.emulator().keyPress(key, down); }

    std::string printed()
    {
        if (machine.ended() && !drained_) {
            printed_ += text_.convert(machine.drainOutput());
            drained_ = true;
        }
        return printed_;
    }

    /* The screen kept as <name>.png in the --crypt-png folder, when one
     * was named; nothing otherwise. */
    void png(const std::string &name)
    {
        auto it = options().find("png");
        if (it == options().end() || it->second.empty()) return;
        ms0515::app::Screen screen;
        screen.render(machine.emulator(), 0);
        ms0515::app::saveScreenshot(screen, (fs::path(it->second) / (name + ".png")).string());
    }

    const uint8_t *vram() { return board_get_vram(&ms0515::internal::board(machine.emulator())); }
    const uint8_t *cell(int c, int y) { return vram() + y * 80 + c * 2; }

    /* The view's window, pixel and attribute bytes, for comparing frames. */
    std::vector<uint8_t> view()
    {
        std::vector<uint8_t> out;
        for (int y = kViewTop; y < kViewTop + kViewRows; ++y)
            for (int c = 0; c < kViewCells; ++c) { out.push_back(cell(c, y)[0]); out.push_back(cell(c, y)[1]); }
        return out;
    }

private:
    fs::path dir_;
    ms0515::Typist typist_;
    ms0515::run::ConsoleText text_{ms0515::run::ConsoleText::Reader::plain};
    std::string printed_;
    bool drained_ = false;
};

}  // namespace crypt

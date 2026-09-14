#include "CellwarGame.hpp"
#include <fstream>
using cellwar::CellwarGame;
static char letter(uint8_t c)
{
    if (!(c & cellwar::Unit)) return '.';
    return ((c & cellwar::Own) ? "iabf" : "IABF")[c & 3];
}
TEST_CASE("sim")
{
    const int rules = std::stoi(cellwar::optOr("rules", "0"));
    const int seed = std::stoi(cellwar::optOr("seed", "1"));
    const std::string tag = "sim_" + std::to_string(rules) + "_" + std::to_string(seed);
    CellwarGame g(tag.c_str(), true, 200);
    g.awaitTitle();
    g.settle(600);
    REQUIRE(g.peek16("MODE") == 1);
    g.poke16("SEED", static_cast<uint16_t>(seed));
    g.poke16("RULES", static_cast<uint16_t>(rules));
    g.poke16("FAST", 1);
    std::ofstream log(std::string("C:/Users/voron/AppData/Local/Temp/claude/C--Users-voron-ms0515/e6128846-9006-4d01-bff5-98e2c1db6cf4/scratchpad/") + tag + ".log");
    int last = 0, maxg = 1, maxr = 1, blood = 0, pg = 1, pr = 1;
    for (long i = 0; i < 400000; ++i) {
        g.step();
        if (g.peek16("MODE") != 1) { log << "MODE " << g.peek16("MODE") << std::endl; break; }
        const int r = g.peek16("ROUND");
        const int cg = g.count(false), cr = g.count(true);
        if (!blood && (cg < pg || cr < pr)) blood = r;
        pg = cg; pr = cr;
        if (cg > maxg) maxg = cg;
        if (cr > maxr) maxr = cr;
        if (r != last) {
            last = r;
            log << "round " << r << " green " << cg << " red " << cr << std::endl;
            if (r % 10 == 0)
                for (int y = 0; y < 10; ++y) {
                    for (int x = 0; x < 20; ++x) log << letter(g.cell(x, y)) << ' ';
                    log << std::endl;
                }
        }
        if (g.peek16("GSTATE") != 0) {
            log << "END round " << r << " gstate " << g.peek16("GSTATE") << " green " << cg << " red " << cr
                << " blood " << blood << " maxg " << maxg << " maxr " << maxr << std::endl;
            break;
        }
        if (r > 150) { log << "CAP round " << r << " green " << cg << " red " << cr << " blood " << blood << " maxg " << maxg << " maxr " << maxr << std::endl; break; }
    }
}

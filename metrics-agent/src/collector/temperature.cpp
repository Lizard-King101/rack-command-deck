#include "collector.h"
#include <fstream>
#include <filesystem>
#include <cmath>

float collector::temperature() {
    namespace fs = std::filesystem;
    const std::string base = "/sys/class/thermal";

    for (auto& entry : fs::directory_iterator(base)) {
        auto name = entry.path().filename().string();
        if (name.rfind("thermal_zone", 0) != 0) continue;

        std::ifstream type_f(entry.path() / "type");
        std::string type;
        std::getline(type_f, type);
        // Prefer cpu-thermal or the first zone if no cpu-thermal found
        if (type != "cpu-thermal" && type != "cpu_thermal" && type != "soc-thermal")
            continue;

        std::ifstream temp_f(entry.path() / "temp");
        int millic = 0;
        if (temp_f >> millic)
            return millic / 1000.f;
    }
    // Fallback: read first zone
    std::ifstream f(base + "/thermal_zone0/temp");
    int millic = 0;
    if (f >> millic) return millic / 1000.f;
    return std::nanf("");
}

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "app/Application.h"
#include "util/StringUtil.h"

namespace {

void PrintUsage() {
    std::cout << "Usage: PatronAnimation [monitor] [options]\n"
              << "\n"
              << "  monitor              0-indexed monitor (0 = primary, 1 = second). Default: 1\n"
              << "  --monitor <n>        Same as the positional argument\n"
              << "  --config <path>      Path to pattern_config.json. Default: auto-resolved\n"
              << "                       relative to the executable directory\n"
              << "  --anim <path>        Path to animation .txt file. Default: auto-resolved\n"
              << "                       from animations/ directory\n"
              << "  --calibrate          Start directly in calibration mode\n"
              << "  --help               Show this message\n"
              << std::endl;
}

bool IsInteger(const char* text) {
    if (!text || !*text) return false;
    const char* p = text;
    if (*p == '-' || *p == '+') ++p;
    if (!*p) return false;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    AppOptions options;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            PrintUsage();
            return 0;
        }

        if (std::strcmp(arg, "--calibrate") == 0) {
            options.startInCalibration = true;
            continue;
        }

        if (std::strcmp(arg, "--config") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --config requires a path." << std::endl;
                return 1;
            }
            options.configPath = AnsiToWide(argv[++i]);
            continue;
        }

        if (std::strcmp(arg, "--anim") == 0 || std::strcmp(arg, "--animation") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --anim requires a file path." << std::endl;
                return 1;
            }
            options.animationPath = argv[++i];
            continue;
        }

        if (std::strcmp(arg, "--monitor") == 0) {
            if (i + 1 >= argc || !IsInteger(argv[i + 1])) {
                std::cerr << "Error: --monitor requires an integer index." << std::endl;
                return 1;
            }
            options.monitorIndex = std::atoi(argv[++i]);
            continue;
        }

        // Backwards compatible: a bare integer is the monitor index.
        if (IsInteger(arg)) {
            options.monitorIndex = std::atoi(arg);
            continue;
        }

        std::cerr << "Error: unrecognised argument '" << arg << "'." << std::endl;
        PrintUsage();
        return 1;
    }

    std::cout << "============================================" << std::endl;
    std::cout << "      Patrón Animation Engine v2.0          " << std::endl;
    std::cout << "============================================" << std::endl;

    Application app;
    if (!app.Initialize(options)) {
        std::cerr << "Fatal Error: Failed to initialize application." << std::endl;
        return 1;
    }

    app.Run();

    return 0;
}

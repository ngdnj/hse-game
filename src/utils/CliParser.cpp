#include "utils/CliParser.hpp"
#include <cstdlib>
#include <iostream>
#include <cstring>

static std::string_view skipPrefix(std::string_view arg, std::string_view prefix) {
    if (arg.starts_with(prefix)) return arg.substr(prefix.size());
    return {};
}

CliOptions parseArgs(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help") {
            std::cout << "Usage: game [--ai] [--model=<name>]\n"
                         "  --ai          Enable AI controller (requires Ollama running)\n"
                         "  --model=<name> Ollama model (default: qwen2.5:0.5b)\n";
            std::exit(0);
        }
        if (arg == "--ai") {
            opts.aiEnabled = true;
        } else if (auto val = skipPrefix(arg, "--model="); !val.empty()) {
            opts.modelName = std::string(val);
        }
    }
    return opts;
}
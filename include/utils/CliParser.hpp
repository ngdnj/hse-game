#pragma once
#include <string>
#include <optional>

struct CliOptions {
    bool aiEnabled = false;
    std::string modelName = "qwen2.5:0.5b";
};

CliOptions parseArgs(int argc, char* argv[]);
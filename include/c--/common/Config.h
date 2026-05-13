#pragma once

#include <string>

namespace cminus {

enum class Stage {
    Lex,
    Parse,
    IR,
    IDE,
    All
};

struct Config {
    std::string inputFile;
    std::string outputDir;
    Stage stage = Stage::All;
    bool showHelp = false;
};

Config parseArgs(int argc, char** argv);
std::string stageToString(Stage stage);
void printHelp();

} // namespac
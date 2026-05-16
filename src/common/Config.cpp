#include "c--/common/Common.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace cminus {

namespace {

bool isStageName(const std::string& text) {
    return text == "lexer" || text == "lex" ||
           text == "parser" || text == "parse" ||
           text == "ir" ||
           text == "ide" ||
           text == "all";
}

Stage parseStageName(const std::string& text) {
    if (text == "lexer" || text == "lex") {
        return Stage::Lex;
    }
    if (text == "parser" || text == "parse") {
        return Stage::Parse;
    }
    if (text == "ir") {
        return Stage::IR;
    }
    if (text == "ide") {
        return Stage::IDE;
    }
    if (text == "all") {
        return Stage::All;
    }
    throw std::runtime_error("unknown mode: " + text);
}

} // namespace

Config parseArgs(int argc, char** argv) {
    Config config;

    if (argc <= 1) {
        config.showHelp = true;
        return config;
    }

    int index = 1;
    std::string first = argv[index];
    if (first == "-h" || first == "--help") {
        config.showHelp = true;
        return config;
    }

    if (isStageName(first)) {
        config.stage = parseStageName(first);
        index++;
    }

    if (index >= argc) {
        throw std::runtime_error("missing input file.");
    }

    config.inputFile = argv[index++];

    while (index < argc) {
        std::string arg = argv[index];
        if (arg == "-o" || arg == "--output") {
            index++;
            if (index >= argc) {
                throw std::runtime_error("missing output directory after " + arg);
            }
            config.outputDir = argv[index++];
            continue;
        }

        throw std::runtime_error("unknown argument: " + arg);
    }

    return config;
}

std::string stageToString(Stage stage) {
    switch (stage) {
        case Stage::Lex:
            return "lexer";
        case Stage::Parse:
            return "parser";
        case Stage::IR:
            return "ir";
        case Stage::IDE:
            return "ide";
        case Stage::All:
            return "all";
    }
    return "unknown";
}

void printHelp() {
    std::cout
        << "Usage:\n"
        << "  c--compiler [lexer|parser|ir|ide|all] <file> [-o output_dir]\n"
        << "  c--compiler <file> [-o output_dir]\n"
        << "\n"
        << "Modes:\n"
        << "  lexer   Run lexical analysis only and write token.txt.\n"
        << "  parser  Run lexer + parser and write token.txt, reduce.txt, ast.txt.\n"
        << "  ir      Run lexer + parser + IR generation and write output.ll.\n"
        << "  ide     Open the C-- terminal editor for the file.\n"
        << "  all     Same as ir. This is the default when mode is omitted.\n"
        << "\n"
        << "Examples:\n"
        << "  c--compiler tests/ok_001_minimal_return.sy\n"
        << "  c--compiler lexer tests/ok_001_minimal_return.sy -o output/lex_case\n"
        << "  c--compiler parser tests/ok_001_minimal_return.sy\n"
        << "  c--compiler ide tests/ok_001_minimal_return.sy\n";
}

} // namespace cminus

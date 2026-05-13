#pragma once

#include <memory>
#include <string>
#include <vector>

#include "c--/parser/AST.h"

namespace cminus {

enum class TokenType {
    Keyword,
    Identifier,
    Integer,
    Float,
    Operator,
    Separator,
    EndOfFile,
    Unknown
};

struct Token {
    std::string lexeme;
    TokenType type;
    std::string grammar;
    std::string attr;
    int line = 1;
    int column = 1;
};

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

struct LexResult {
    bool success = false;
    std::vector<Token> tokens;
    std::string errorMessage;
};

struct ParseResult {
    bool success = false;
    std::unique_ptr<ASTNode> root;
    std::vector<std::string> reduceLogs;
    std::string errorMessage;
};

struct IRResult {
    bool success = false;
    std::string irText;
    std::string errorMessage;
};

std::string tokenTypeToString(TokenType type);

Config parseArgs(int argc, char** argv);
std::string stageToString(Stage stage);
void printHelp();

std::string readTextFile(const std::string& path);
void writeTextFile(const std::string& path, const std::string& content);

std::string createTimestampOutputDir();
void ensureDirectory(const std::string& dir);
void copyInputFile(const std::string& inputPath, const std::string& outputDir);

void writeTokens(const std::vector<Token>& tokens, const std::string& path);
void writeLines(const std::vector<std::string>& lines, const std::string& path);
void writeASTFile(const ASTNode* root, const std::string& path);
void writeRunInfo(const Config& config, const std::string& outputDir, bool success, const std::string& errorStage);

} // namespace cminus

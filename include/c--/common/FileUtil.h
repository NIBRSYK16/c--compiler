#pragma once

#include <string>
#include <vector>

#include "cminus/common/Token.h"
#include "cminus/parser/AST.h"

namespace cminus {

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

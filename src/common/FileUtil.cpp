#include "c--/common/Common.h"

#include <cerrno>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace cminus {

namespace {

bool directoryExists(const std::string& dir) {
    struct stat info;
    return stat(dir.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::string parentDir(const std::string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return "";
    }
    if (slash == 0) {
        return "/";
    }
    return path.substr(0, slash);
}

std::string baseName(const std::string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

void ensureParentDirectory(const std::string& path) {
    std::string parent = parentDir(path);
    if (!parent.empty()) {
        ensureDirectory(parent);
    }
}

} // namespace

std::string readTextFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void writeTextFile(const std::string& path, const std::string& content) {
    ensureParentDirectory(path);

    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot write file: " + path);
    }

    output << content;
}

void ensureDirectory(const std::string& dir) {
    if (dir.empty() || dir == ".") {
        return;
    }
    if (directoryExists(dir)) {
        return;
    }

    std::string parent = parentDir(dir);
    if (!parent.empty() && parent != dir) {
        ensureDirectory(parent);
    }

    if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
        throw std::runtime_error("cannot create directory: " + dir);
    }
}

std::string createTimestampOutputDir() {
    ensureDirectory("output");

    std::time_t now = std::time(NULL);
    std::tm* local = std::localtime(&now);
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "output/result_%Y%m%d_%H%M%S", local);

    std::string dir = buffer;
    if (!directoryExists(dir)) {
        ensureDirectory(dir);
        return dir;
    }

    for (int i = 1; i < 1000; i++) {
        std::ostringstream candidate;
        candidate << dir << "_" << i;
        if (!directoryExists(candidate.str())) {
            ensureDirectory(candidate.str());
            return candidate.str();
        }
    }

    throw std::runtime_error("cannot create unique output directory.");
}

void copyInputFile(const std::string& inputPath, const std::string& outputDir) {
    std::string content = readTextFile(inputPath);
    std::string name = baseName(inputPath);
    if (name.empty()) {
        name = "input.sy";
    }
    writeTextFile(outputDir + "/" + name, content);
}

void writeTokens(const std::vector<Token>& tokens, const std::string& path) {
    std::ostringstream out;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i].type == TokenType::EndOfFile) {
            continue;
        }
        out << tokens[i].lexeme << '\t'
            << '<' << tokenTypeToString(tokens[i].type) << ',' << tokens[i].attr << '>'
            << '\n';
    }
    writeTextFile(path, out.str());
}

void writeLines(const std::vector<std::string>& lines, const std::string& path) {
    std::ostringstream out;
    for (size_t i = 0; i < lines.size(); i++) {
        out << lines[i] << '\n';
    }
    writeTextFile(path, out.str());
}

void writeASTFile(const ASTNode* root, const std::string& path) {
    std::ostringstream out;
    printAST(root, out, 0);
    writeTextFile(path, out.str());
}

void writeRunInfo(const Config& config, const std::string& outputDir, bool success, const std::string& errorStage) {
    std::ostringstream out;
    out << "input: " << config.inputFile << '\n';
    out << "stage: " << stageToString(config.stage) << '\n';
    out << "output_dir: " << outputDir << '\n';
    out << "success: " << (success ? "true" : "false") << '\n';
    if (!errorStage.empty()) {
        out << "error_stage: " << errorStage << '\n';
    }
    writeTextFile(outputDir + "/run_info.txt", out.str());
}

} // namespace cminus

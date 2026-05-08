#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "c--/common/Diagnostic.h"
#include "c--/lexer/lexer.h"
#include "c--/parser/parser.h"

namespace {

const char* RESET = "\033[0m";
const char* DIM = "\033[2m";
const char* GREEN = "\033[32m";
const char* YELLOW = "\033[33m";
const char* BLUE = "\033[34m";
const char* MAGENTA = "\033[35m";
const char* CYAN = "\033[36m";
const char* BOLD = "\033[1m";

struct EditorState {
    std::vector<std::string> lines;
    std::string path;
    bool liveAnalyze = true;
    bool useColor = true;
};

std::string joinLines(const std::vector<std::string>& lines) {
    std::ostringstream output;
    for (size_t i = 0; i < lines.size(); i++) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
    return output.str();
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        lines.push_back(line);
    }
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

std::string stripUtf8Bom(const std::string& text) {
    if (text.size() >= 3 &&
        (unsigned char)text[0] == 0xEF &&
        (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        return text.substr(3);
    }
    return text;
}

std::string normalizeConsoleLine(const std::string& text) {
    std::string result = stripUtf8Bom(text);
    size_t commandStart = result.find(':');
    if (commandStart != std::string::npos && commandStart <= 6) {
        return result.substr(commandStart);
    }
    return result;
}

std::string readTextFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return stripUtf8Bom(buffer.str());
}

void writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot write file: " + path);
    }
    output << content;
}

std::string colorize(const EditorState& state, const std::string& text, const char* color) {
    if (!state.useColor) {
        return text;
    }
    return std::string(color) + text + RESET;
}

const char* tokenColor(cminus::TokenType type) {
    switch (type) {
        case cminus::TokenType::Keyword:
            return MAGENTA;
        case cminus::TokenType::Identifier:
            return CYAN;
        case cminus::TokenType::Integer:
        case cminus::TokenType::Float:
            return YELLOW;
        case cminus::TokenType::Operator:
            return BLUE;
        case cminus::TokenType::Separator:
            return GREEN;
        default:
            return RESET;
    }
}

std::string highlightLine(const EditorState& state,
                          const std::string& line,
                          int lineNumber,
                          const std::vector<cminus::Token>& tokens) {
    std::ostringstream output;
    int column = 1;

    for (size_t i = 0; i < tokens.size(); i++) {
        const cminus::Token& token = tokens[i];
        if (token.line != lineNumber || token.type == cminus::TokenType::EndOfFile) {
            continue;
        }

        if (token.column > column) {
            output << line.substr(column - 1, token.column - column);
        }

        std::string lexeme = token.lexeme;
        output << colorize(state, lexeme, tokenColor(token.type));
        column = token.column + (int)lexeme.size();
    }

    if (column <= (int)line.size()) {
        output << line.substr(column - 1);
    }

    return output.str();
}

void printDiagnostics(const std::vector<cminus::Diagnostic>& diagnostics, int limit) {
    for (size_t i = 0; i < diagnostics.size() && (int)i < limit; i++) {
        std::cerr << cminus::renderDiagnostic(diagnostics[i]) << '\n';
    }
    if ((int)diagnostics.size() > limit) {
        std::cerr << "... " << (diagnostics.size() - limit) << " more diagnostic(s)\n";
    }
}

bool analyze(const EditorState& state, bool verbose) {
    std::string source = joinLines(state.lines);
    cminus::Lexer lexer;
    cminus::LexResult lexResult = lexer.tokenize(source);

    cminus::Parser parser;
    cminus::TokenStreamSummary syntaxSummary = parser.precheckTokenStream(lexResult.tokens);

    int errors = 0;
    int warnings = 0;
    for (size_t i = 0; i < lexResult.diagnostics.size(); i++) {
        if (lexResult.diagnostics[i].severity == cminus::DiagnosticSeverity::Error) {
            errors++;
        } else if (lexResult.diagnostics[i].severity == cminus::DiagnosticSeverity::Warning) {
            warnings++;
        }
    }
    for (size_t i = 0; i < syntaxSummary.diagnostics.size(); i++) {
        if (syntaxSummary.diagnostics[i].severity == cminus::DiagnosticSeverity::Error) {
            errors++;
        } else if (syntaxSummary.diagnostics[i].severity == cminus::DiagnosticSeverity::Warning) {
            warnings++;
        }
    }

    std::cout << colorize(state, "[analysis] ", BOLD)
              << "tokens=" << lexResult.tokens.size()
              << ", lexer=" << (lexResult.success ? "ok" : "error")
              << ", syntax-precheck=" << (syntaxSummary.success ? "ok" : "error")
              << ", errors=" << errors
              << ", warnings=" << warnings
              << ", paren_depth=" << syntaxSummary.maxParenDepth
              << ", brace_depth=" << syntaxSummary.maxBraceDepth
              << '\n';

    if (verbose) {
        printDiagnostics(lexResult.diagnostics, 20);
        printDiagnostics(syntaxSummary.diagnostics, 20);
    } else {
        if (!lexResult.diagnostics.empty()) {
            std::cerr << cminus::renderDiagnostic(lexResult.diagnostics[0]) << '\n';
        } else if (!syntaxSummary.diagnostics.empty()) {
            std::cerr << cminus::renderDiagnostic(syntaxSummary.diagnostics[0]) << '\n';
        }
    }

    return errors == 0;
}

void showBuffer(const EditorState& state, int fromLine, int toLine) {
    cminus::Lexer lexer;
    cminus::LexResult result = lexer.tokenize(joinLines(state.lines));

    int first = std::max(1, fromLine);
    int last = std::min((int)state.lines.size(), toLine);
    for (int line = first; line <= last; line++) {
        std::ostringstream gutter;
        gutter.width(4);
        gutter << line;
        std::cout << colorize(state, gutter.str(), DIM) << " | "
                  << highlightLine(state, state.lines[line - 1], line, result.tokens)
                  << '\n';
    }
}

void printHelp() {
    std::cout
        << "Commands:\n"
        << "  :open <file>          open a .sy file\n"
        << "  :save [file]          save current buffer\n"
        << "  :show [from] [to]     show highlighted source\n"
        << "  :insert <line> <text> insert text before line\n"
        << "  :append <text>        append one line\n"
        << "  :replace <line> <text> replace one line\n"
        << "  :delete <line>        delete one line\n"
        << "  :analyze              run lexer + syntax precheck\n"
        << "  :tokens               print token table\n"
        << "  :live on|off          toggle analyze-after-edit\n"
        << "  :color on|off         toggle ANSI colors\n"
        << "  :help                 show this help\n"
        << "  :quit                 exit\n";
}

int parseInt(const std::string& text, int fallback) {
    if (text.empty()) {
        return fallback;
    }
    return std::atoi(text.c_str());
}

std::string restAfterTwoWords(const std::string& line) {
    size_t first = line.find(' ');
    if (first == std::string::npos) {
        return "";
    }
    size_t second = line.find(' ', first + 1);
    if (second == std::string::npos) {
        return "";
    }
    return line.substr(second + 1);
}

void printTokens(const EditorState& state) {
    cminus::Lexer lexer;
    cminus::LexResult result = lexer.tokenize(joinLines(state.lines));
    for (size_t i = 0; i < result.tokens.size(); i++) {
        const cminus::Token& token = result.tokens[i];
        std::cout << token.line << ':' << token.column << '\t'
                  << cminus::tokenTypeToString(token.type) << '\t'
                  << token.grammar << '\t'
                  << token.attr << '\t'
                  << token.lexeme << '\n';
    }
}

void afterEdit(const EditorState& state) {
    if (state.liveAnalyze) {
        analyze(state, false);
    }
}

void handleCommand(EditorState& state, const std::string& line) {
    std::istringstream input(line);
    std::string command;
    input >> command;

    if (command == ":open") {
        std::string path;
        input >> path;
        if (path.empty()) {
            std::cerr << "missing file path\n";
            return;
        }
        state.lines = splitLines(readTextFile(path));
        state.path = path;
        std::cout << "opened " << path << " (" << state.lines.size() << " line(s))\n";
        afterEdit(state);
    } else if (command == ":save") {
        std::string path;
        input >> path;
        if (path.empty()) {
            path = state.path;
        }
        if (path.empty()) {
            std::cerr << "missing file path\n";
            return;
        }
        writeTextFile(path, joinLines(state.lines));
        state.path = path;
        std::cout << "saved " << path << '\n';
    } else if (command == ":show") {
        std::string firstText;
        std::string lastText;
        input >> firstText >> lastText;
        int first = parseInt(firstText, 1);
        int last = parseInt(lastText, (int)state.lines.size());
        showBuffer(state, first, last);
    } else if (command == ":insert") {
        std::string lineText;
        input >> lineText;
        int index = parseInt(lineText, 1);
        std::string text = restAfterTwoWords(line);
        index = std::max(1, std::min(index, (int)state.lines.size() + 1));
        state.lines.insert(state.lines.begin() + index - 1, text);
        afterEdit(state);
    } else if (command == ":append") {
        size_t pos = line.find(' ');
        state.lines.push_back(pos == std::string::npos ? "" : line.substr(pos + 1));
        afterEdit(state);
    } else if (command == ":replace") {
        std::string lineText;
        input >> lineText;
        int index = parseInt(lineText, -1);
        if (index < 1 || index > (int)state.lines.size()) {
            std::cerr << "line out of range\n";
            return;
        }
        state.lines[index - 1] = restAfterTwoWords(line);
        afterEdit(state);
    } else if (command == ":delete") {
        std::string lineText;
        input >> lineText;
        int index = parseInt(lineText, -1);
        if (index < 1 || index > (int)state.lines.size()) {
            std::cerr << "line out of range\n";
            return;
        }
        state.lines.erase(state.lines.begin() + index - 1);
        if (state.lines.empty()) {
            state.lines.push_back("");
        }
        afterEdit(state);
    } else if (command == ":analyze") {
        analyze(state, true);
    } else if (command == ":tokens") {
        printTokens(state);
    } else if (command == ":live") {
        std::string mode;
        input >> mode;
        state.liveAnalyze = mode != "off";
        std::cout << "live analysis " << (state.liveAnalyze ? "on" : "off") << '\n';
    } else if (command == ":color") {
        std::string mode;
        input >> mode;
        state.useColor = mode != "off";
        std::cout << "color " << (state.useColor ? "on" : "off") << '\n';
    } else if (command == ":help") {
        printHelp();
    } else {
        std::cerr << "unknown command: " << command << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    EditorState state;
    state.lines.push_back("int main() {");
    state.lines.push_back("    return 0;");
    state.lines.push_back("}");

    try {
        if (argc >= 2) {
            state.path = argv[1];
            state.lines = splitLines(readTextFile(state.path));
        }

        if (std::getenv("NO_COLOR") != NULL) {
            state.useColor = false;
        }

        std::cout << "C-- live editor. Type :help for commands, :quit to exit.\n";
        analyze(state, false);

        std::string line;
        while (true) {
            std::cout << "c--edit> ";
            if (!std::getline(std::cin, line)) {
                break;
            }
            line = normalizeConsoleLine(line);
            if (line == ":quit" || line == ":q") {
                break;
            }
            if (line.empty()) {
                continue;
            }
            if (line[0] == ':') {
                handleCommand(state, line);
            } else {
                state.lines.push_back(line);
                afterEdit(state);
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "[live_editor] " << error.what() << '\n';
        return 1;
    }

    return 0;
}

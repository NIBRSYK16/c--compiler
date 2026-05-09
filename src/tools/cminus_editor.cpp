#include <ncurses.h>

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "c--/common/Token.h"
#include "c--/lexer/lexer.h"
#include "c--/parser/parser.h"

#define CTRL_KEY(ch) ((ch) & 0x1f)

#ifndef BUTTON5_PRESSED
#define BUTTON5_PRESSED ((mmask_t)002L << ((5 - 1) * 6))
#endif

namespace {

enum ColorPair {
    COLOR_NORMAL_TEXT = 1,
    COLOR_KEYWORD_TEXT = 2,
    COLOR_ID_TEXT = 3,
    COLOR_NUMBER_TEXT = 4,
    COLOR_OPERATOR_TEXT = 5,
    COLOR_SEPARATOR_TEXT = 6,
    COLOR_ERROR_TEXT = 7,
    COLOR_HEADER_TEXT = 8,
    COLOR_STATUS_TEXT = 9,
    COLOR_LINE_NO_TEXT = 10
};

enum ActivePane {
    PANE_EDITOR,
    PANE_TOKENS,
    PANE_PARSER
};

struct TokenSpan {
    int line = 1;
    int column = 1;
    int length = 0;
    int tokenLine = 0;
    cminus::TokenType type = cminus::TokenType::Unknown;
    std::string lexeme;
    std::string grammar;
};

struct ErrorSpan {
    int line = 1;
    int column = 1;
    int length = 1;
    std::string message;
};

struct AnalysisView {
    bool lexOk = false;
    bool parseOk = false;
    std::string lexError;
    std::string parseError;
    std::vector<std::string> tokenLines;
    std::vector<std::string> syntaxLines;
    std::vector<TokenSpan> spans;
    std::vector<ErrorSpan> errors;
};

std::string readFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) {
        return "";
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool writeFile(const std::string& path, const std::string& text, std::string& error) {
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
    if (!output) {
        error = "cannot write file: " + path;
        return false;
    }

    output << text;
    return true;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;

    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\n') {
            lines.push_back(current);
            current.clear();
        } else if (text[i] != '\r') {
            current.push_back(text[i]);
        }
    }

    lines.push_back(current);
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::ostringstream out;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) {
            out << '\n';
        }
        out << lines[i];
    }
    return out.str();
}

std::string tokenTypeName(cminus::TokenType type) {
    switch (type) {
        case cminus::TokenType::Keyword:
            return "KW";
        case cminus::TokenType::Identifier:
            return "IDN";
        case cminus::TokenType::Integer:
            return "INT";
        case cminus::TokenType::Float:
            return "FLOAT";
        case cminus::TokenType::Operator:
            return "OP";
        case cminus::TokenType::Separator:
            return "SE";
        case cminus::TokenType::EndOfFile:
            return "EOF";
        default:
            return "UNKNOWN";
    }
}

int colorForToken(cminus::TokenType type) {
    switch (type) {
        case cminus::TokenType::Keyword:
            return COLOR_KEYWORD_TEXT;
        case cminus::TokenType::Identifier:
            return COLOR_ID_TEXT;
        case cminus::TokenType::Integer:
        case cminus::TokenType::Float:
            return COLOR_NUMBER_TEXT;
        case cminus::TokenType::Operator:
            return COLOR_OPERATOR_TEXT;
        case cminus::TokenType::Separator:
            return COLOR_SEPARATOR_TEXT;
        default:
            return COLOR_NORMAL_TEXT;
    }
}

void collectAST(const cminus::ASTNode* node, int depth, std::vector<std::string>& out, int limit) {
    if (node == NULL || (int)out.size() >= limit) {
        return;
    }

    std::string line(depth * 2, ' ');
    line += node->name;
    if (!node->value.empty()) {
        line += " value=" + node->value;
    }
    out.push_back(line);

    for (size_t i = 0; i < node->children.size(); i++) {
        collectAST(node->children[i].get(), depth + 1, out, limit);
    }
}

void appendWrappedLines(const std::string& text, std::vector<std::string>& lines) {
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
}

bool parseErrorPosition(const std::string& line, int& errorLine, int& errorColumn) {
    if (std::sscanf(line.c_str(), "Lexical error at line %d, column %d", &errorLine, &errorColumn) == 2) {
        return true;
    }
    if (std::sscanf(line.c_str(), "Syntax error at line %d, column %d", &errorLine, &errorColumn) == 2) {
        return true;
    }
    return false;
}

int tokenLengthAt(const std::vector<cminus::Token>& tokens, int line, int column) {
    for (size_t i = 0; i < tokens.size(); i++) {
        const cminus::Token& token = tokens[i];
        if (token.type == cminus::TokenType::EndOfFile) {
            continue;
        }
        if (token.line == line && token.column == column) {
            return std::max(1, (int)token.lexeme.size());
        }
    }
    return 1;
}

void addErrorSpansFromMessage(
    AnalysisView& view,
    const std::string& message,
    const std::vector<cminus::Token>& tokens
) {
    std::istringstream input(message);
    std::string line;
    while (std::getline(input, line)) {
        int errorLine = 0;
        int errorColumn = 0;
        if (!parseErrorPosition(line, errorLine, errorColumn)) {
            continue;
        }

        ErrorSpan span;
        span.line = errorLine;
        span.column = errorColumn;
        span.length = tokenLengthAt(tokens, errorLine, errorColumn);
        span.message = line;
        view.errors.push_back(span);
    }
}

AnalysisView analyzeSource(const std::string& source) {
    AnalysisView view;

    cminus::Lexer lexer;
    cminus::LexResult lex = lexer.tokenize(source);
    view.lexOk = lex.success;
    view.lexError = lex.errorMessage;

    for (size_t i = 0; i < lex.tokens.size(); i++) {
        const cminus::Token& token = lex.tokens[i];
        std::ostringstream line;
        line << token.line << ":" << token.column << "  "
             << token.lexeme << "  <" << tokenTypeName(token.type) << "," << token.attr << ">"
             << "  grammar=" << token.grammar;
        view.tokenLines.push_back(line.str());

        if (token.type != cminus::TokenType::EndOfFile) {
            TokenSpan span;
            span.line = token.line;
            span.column = token.column;
            span.length = (int)token.lexeme.size();
            span.tokenLine = (int)view.tokenLines.size() - 1;
            span.type = token.type;
            span.lexeme = token.lexeme;
            span.grammar = token.grammar;
            view.spans.push_back(span);
        }
    }

    if (!lex.success) {
        addErrorSpansFromMessage(view, lex.errorMessage, lex.tokens);
        view.syntaxLines.push_back("Parser skipped because lexer has errors.");
        appendWrappedLines(lex.errorMessage, view.syntaxLines);
        return view;
    }

    try {
        cminus::Parser parser;
        cminus::ParseResult parsed = parser.parse(lex.tokens);
        view.parseOk = parsed.success;

        if (!parsed.success) {
            view.parseError = parsed.errorMessage;
            addErrorSpansFromMessage(view, parsed.errorMessage, lex.tokens);
            view.syntaxLines.push_back("Parse: ERROR");
            view.syntaxLines.push_back(parsed.errorMessage);
            return view;
        }

        view.syntaxLines.push_back("Parse: OK");
        view.syntaxLines.push_back("AST:");
        collectAST(parsed.root.get(), 0, view.syntaxLines, 80);
        view.syntaxLines.push_back("");
        view.syntaxLines.push_back("Reductions: " + std::to_string((long long)parsed.reduceLogs.size()));

        size_t count = std::min<size_t>(parsed.reduceLogs.size(), 80);
        for (size_t i = 0; i < count; i++) {
            view.syntaxLines.push_back(parsed.reduceLogs[i]);
        }
    } catch (const std::exception& error) {
        view.parseOk = false;
        view.parseError = error.what();
        view.syntaxLines.push_back("Parse: INTERNAL ERROR");
        view.syntaxLines.push_back(error.what());
    }

    return view;
}

void initColors() {
    if (!has_colors()) {
        return;
    }

    start_color();
    use_default_colors();
    init_pair(COLOR_NORMAL_TEXT, COLOR_WHITE, -1);
    init_pair(COLOR_KEYWORD_TEXT, COLOR_CYAN, -1);
    init_pair(COLOR_ID_TEXT, COLOR_WHITE, -1);
    init_pair(COLOR_NUMBER_TEXT, COLOR_YELLOW, -1);
    init_pair(COLOR_OPERATOR_TEXT, COLOR_MAGENTA, -1);
    init_pair(COLOR_SEPARATOR_TEXT, COLOR_GREEN, -1);
    init_pair(COLOR_ERROR_TEXT, COLOR_RED, -1);
    init_pair(COLOR_HEADER_TEXT, COLOR_BLACK, COLOR_CYAN);
    init_pair(COLOR_STATUS_TEXT, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_LINE_NO_TEXT, COLOR_BLUE, -1);
}

void enableTerminalMouse() {
    // ncurses on some macOS terminals enables only basic click reporting.
    // These xterm modes make click, drag and SGR mouse coordinates more likely to be sent.
    std::printf("\033[?1000h\033[?1002h\033[?1006h");
    std::fflush(stdout);
}

void disableTerminalMouse() {
    std::printf("\033[?1006l\033[?1002l\033[?1000l");
    std::fflush(stdout);
}

void drawFill(int y, int x, int width, int color) {
    if (width <= 0) {
        return;
    }

    attron(COLOR_PAIR(color));
    for (int i = 0; i < width; i++) {
        mvaddch(y, x + i, ' ');
    }
    attroff(COLOR_PAIR(color));
}

void drawText(int y, int x, int width, const std::string& text, int color, bool bold = false, bool reverse = false) {
    if (width <= 0) {
        return;
    }

    drawFill(y, x, width, color);
    attron(COLOR_PAIR(color));
    if (bold) {
        attron(A_BOLD);
    }
    if (reverse) {
        attron(A_REVERSE);
    }

    std::string clipped = text.substr(0, (size_t)width);
    mvaddnstr(y, x, clipped.c_str(), width);

    if (reverse) {
        attroff(A_REVERSE);
    }
    if (bold) {
        attroff(A_BOLD);
    }
    attroff(COLOR_PAIR(color));
}

int clampInt(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

class Editor {
public:
    explicit Editor(const std::string& path)
        : filePath(path), lines(splitLines(readFile(path))), savedLines(lines) {
        if (lines.empty()) {
            lines.push_back("");
            savedLines = lines;
        }
        analyze();
    }

    void run() {
        draw();

        while (running) {
            int key = getch();
            handleKey(key);
            draw();
        }
    }

private:
    std::string filePath;
    std::vector<std::string> lines;
    std::vector<std::string> savedLines;
    AnalysisView analysis;
    int cursorLine = 0;
    int cursorCol = 0;
    int topLine = 0;
    int leftCol = 0;
    int tokenScroll = 0;
    int parserScroll = 0;
    ActivePane activePane = PANE_EDITOR;
    bool modified = false;
    bool running = true;
    std::string message = "Click/F2/F3 focus pane | wheel or [] scroll | Ctrl-S save | Ctrl-Q quit";

    void analyze() {
        analysis = analyzeSource(joinLines(lines));
        clampPanelScrolls();
    }

    void markChanged() {
        modified = true;
        analyze();
    }

    int selectedTokenIndex() const {
        int line = cursorLine + 1;
        int column = cursorCol + 1;
        for (size_t i = 0; i < analysis.spans.size(); i++) {
            const TokenSpan& span = analysis.spans[i];
            int begin = span.column;
            int end = span.column + std::max(1, span.length);
            if (span.line == line && column >= begin && column < end) {
                return (int)i;
            }
        }

        for (size_t i = 0; i < analysis.spans.size(); i++) {
            const TokenSpan& span = analysis.spans[i];
            if (span.line == line && column == span.column + span.length) {
                return (int)i;
            }
        }

        return -1;
    }

    bool syntaxRowMatchesToken(const std::string& row, int tokenIndex) const {
        if (tokenIndex < 0 || tokenIndex >= (int)analysis.spans.size()) {
            return false;
        }

        const TokenSpan& span = analysis.spans[tokenIndex];
        if (!span.lexeme.empty() && row.find(span.lexeme) != std::string::npos) {
            return true;
        }
        if (!span.grammar.empty() && row.find(span.grammar) != std::string::npos) {
            return true;
        }
        if (row.find("#" + span.grammar) != std::string::npos) {
            return true;
        }
        return false;
    }

    int errorIndexAtCursor() const {
        int line = cursorLine + 1;
        int column = cursorCol + 1;
        for (size_t i = 0; i < analysis.errors.size(); i++) {
            const ErrorSpan& error = analysis.errors[i];
            int begin = error.column;
            int end = error.column + std::max(1, error.length);
            if (error.line == line && column >= begin && column <= end) {
                return (int)i;
            }
        }
        return -1;
    }

    void clampCursor() {
        if (lines.empty()) {
            lines.push_back("");
        }

        if (cursorLine < 0) {
            cursorLine = 0;
        }
        if (cursorLine >= (int)lines.size()) {
            cursorLine = (int)lines.size() - 1;
        }

        int lineLength = (int)lines[cursorLine].size();
        if (cursorCol < 0) {
            cursorCol = 0;
        }
        if (cursorCol > lineLength) {
            cursorCol = lineLength;
        }
    }

    int visibleRowsForPane(ActivePane pane) const {
        int contentH = LINES - 1;
        int topPaneH = contentH / 2;
        int bottomPaneH = contentH - topPaneH;
        if (pane == PANE_TOKENS) {
            return std::max(0, topPaneH - 1);
        }
        if (pane == PANE_PARSER) {
            return std::max(0, bottomPaneH - 1);
        }
        return std::max(0, contentH - 2);
    }

    int maxScrollForRows(const std::vector<std::string>& rows, int visibleRows) const {
        return std::max(0, (int)rows.size() - visibleRows);
    }

    void clampPanelScrolls() {
        tokenScroll = clampInt(tokenScroll, 0, maxScrollForRows(analysis.tokenLines, visibleRowsForPane(PANE_TOKENS)));
        parserScroll = clampInt(parserScroll, 0, maxScrollForRows(analysis.syntaxLines, visibleRowsForPane(PANE_PARSER)));
    }

    void keepSelectedTokenVisible() {
        int tokenIndex = selectedTokenIndex();
        if (tokenIndex < 0 || tokenIndex >= (int)analysis.spans.size()) {
            return;
        }

        int row = analysis.spans[tokenIndex].tokenLine;
        int visible = visibleRowsForPane(PANE_TOKENS);
        if (visible > 0 && activePane != PANE_TOKENS) {
            if (row < tokenScroll) {
                tokenScroll = row;
            } else if (row >= tokenScroll + visible) {
                tokenScroll = row - visible + 1;
            }
        }

        int syntaxVisible = visibleRowsForPane(PANE_PARSER);
        if (syntaxVisible > 0 && activePane != PANE_PARSER) {
            for (size_t i = 0; i < analysis.syntaxLines.size(); i++) {
                if (syntaxRowMatchesToken(analysis.syntaxLines[i], tokenIndex)) {
                    if ((int)i < parserScroll) {
                        parserScroll = (int)i;
                    } else if ((int)i >= parserScroll + syntaxVisible) {
                        parserScroll = (int)i - syntaxVisible + 1;
                    }
                    break;
                }
            }
        }

        clampPanelScrolls();
    }

    void scrollActivePane(int amount) {
        if (activePane == PANE_TOKENS) {
            tokenScroll += amount;
        } else if (activePane == PANE_PARSER) {
            parserScroll += amount;
        } else {
            topLine += amount;
        }
        clampPanelScrolls();
        topLine = clampInt(topLine, 0, std::max(0, (int)lines.size() - 1));
    }

    void ensureCursorVisible(int editorHeight, int textWidth) {
        if (editorHeight < 1) {
            return;
        }

        if (cursorLine < topLine) {
            topLine = cursorLine;
        }
        if (cursorLine >= topLine + editorHeight) {
            topLine = cursorLine - editorHeight + 1;
        }
        if (topLine < 0) {
            topLine = 0;
        }

        if (cursorCol < leftCol) {
            leftCol = cursorCol;
        }
        if (cursorCol >= leftCol + textWidth) {
            leftCol = cursorCol - textWidth + 1;
        }
        if (leftCol < 0) {
            leftCol = 0;
        }
    }

    void insertChar(int ch) {
        lines[cursorLine].insert(lines[cursorLine].begin() + cursorCol, (char)ch);
        cursorCol++;
        markChanged();
    }

    std::string leadingSpaces(const std::string& text) const {
        std::string result;
        for (size_t i = 0; i < text.size(); i++) {
            if (text[i] != ' ' && text[i] != '\t') {
                break;
            }
            result.push_back(text[i]);
        }
        return result;
    }

    std::string rtrim(const std::string& text) const {
        size_t end = text.size();
        while (end > 0 && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
            end--;
        }
        return text.substr(0, end);
    }

    std::string indentForNewline(const std::string& left, const std::string& right) const {
        std::string indent = leadingSpaces(left);
        std::string trimmedLeft = rtrim(left);

        if (!trimmedLeft.empty() && trimmedLeft[trimmedLeft.size() - 1] == '{') {
            indent += "  ";
        }

        if (!right.empty() && right[0] == '}' && indent.size() >= 2) {
            indent.erase(indent.size() - 2);
        }

        return indent;
    }

    void insertNewline() {
        std::string left = lines[cursorLine].substr(0, cursorCol);
        std::string right = lines[cursorLine].substr(cursorCol);
        std::string indent = indentForNewline(left, right);

        lines[cursorLine] = left;
        lines.insert(lines.begin() + cursorLine + 1, indent + right);
        cursorLine++;
        cursorCol = (int)indent.size();
        markChanged();
    }

    void backspace() {
        if (cursorCol > 0) {
            lines[cursorLine].erase(lines[cursorLine].begin() + cursorCol - 1);
            cursorCol--;
            markChanged();
            return;
        }

        if (cursorLine > 0) {
            int oldLength = (int)lines[cursorLine - 1].size();
            lines[cursorLine - 1] += lines[cursorLine];
            lines.erase(lines.begin() + cursorLine);
            cursorLine--;
            cursorCol = oldLength;
            markChanged();
        }
    }

    void deleteChar() {
        if (cursorCol < (int)lines[cursorLine].size()) {
            lines[cursorLine].erase(lines[cursorLine].begin() + cursorCol);
            markChanged();
            return;
        }

        if (cursorLine + 1 < (int)lines.size()) {
            lines[cursorLine] += lines[cursorLine + 1];
            lines.erase(lines.begin() + cursorLine + 1);
            markChanged();
        }
    }

    void save() {
        std::string error;
        if (writeFile(filePath, joinLines(lines), error)) {
            savedLines = lines;
            modified = false;
            message = "Saved: " + filePath;
        } else {
            message = error;
        }
    }

    void revert() {
        lines = savedLines;
        cursorLine = 0;
        cursorCol = 0;
        topLine = 0;
        leftCol = 0;
        modified = false;
        message = "Reverted to last saved content";
        analyze();
    }

    void moveCursor(int key) {
        if (key == KEY_LEFT) {
            if (cursorCol > 0) {
                cursorCol--;
            } else if (cursorLine > 0) {
                cursorLine--;
                cursorCol = (int)lines[cursorLine].size();
            }
        } else if (key == KEY_RIGHT) {
            if (cursorCol < (int)lines[cursorLine].size()) {
                cursorCol++;
            } else if (cursorLine + 1 < (int)lines.size()) {
                cursorLine++;
                cursorCol = 0;
            }
        } else if (key == KEY_UP) {
            cursorLine--;
        } else if (key == KEY_DOWN) {
            cursorLine++;
        } else if (key == KEY_HOME) {
            cursorCol = 0;
        } else if (key == KEY_END) {
            cursorCol = (int)lines[cursorLine].size();
        } else if (key == KEY_PPAGE) {
            cursorLine -= std::max(1, LINES / 2);
        } else if (key == KEY_NPAGE) {
            cursorLine += std::max(1, LINES / 2);
        }

        clampCursor();
    }

    void focusPaneByMouse(int y, int x) {
        int contentH = LINES - 1;
        int leftW = COLS / 2;
        int sepX = leftW;
        int topPaneH = contentH / 2;

        if (x < leftW) {
            activePane = PANE_EDITOR;
            if (y >= 1 && y < contentH) {
                cursorLine = topLine + y - 1;
                cursorCol = leftCol + std::max(0, x - 5);
                clampCursor();
            }
            return;
        }

        if (x > sepX && y < topPaneH) {
            activePane = PANE_TOKENS;
            message = "Tokens pane focused. Mouse wheel or PageUp/PageDown scrolls it.";
            return;
        }

        if (x > sepX && y < contentH) {
            activePane = PANE_PARSER;
            message = "Parser pane focused. Mouse wheel or PageUp/PageDown scrolls it.";
        }
    }

    void handleMouse() {
        MEVENT event;
        if (getmouse(&event) != OK) {
            message = "Mouse event was not decoded. Try F2/F3, [], PageUp/PageDown.";
            return;
        }

        focusPaneByMouse(event.y, event.x);

        if ((event.bstate & BUTTON4_PRESSED) != 0) {
            scrollActivePane(-3);
            return;
        }
        if ((event.bstate & BUTTON5_PRESSED) != 0) {
            scrollActivePane(3);
            return;
        }

        if ((event.bstate & BUTTON1_PRESSED) != 0 || (event.bstate & BUTTON1_CLICKED) != 0) {
            clampPanelScrolls();
            return;
        }

        std::ostringstream out;
        out << "Mouse event not mapped: " << (long)event.bstate << ". Use [] if wheel fails.";
        message = out.str();
    }

    void handleDecodedMouse(int button, int x, int y) {
        focusPaneByMouse(y, x);
        if ((button & 64) != 0) {
            if ((button & 1) == 0) {
                scrollActivePane(-3);
            } else {
                scrollActivePane(3);
            }
        } else {
            message = "Mouse focus changed. Wheel or [] scrolls the focused pane.";
        }
    }

    bool handleEscapeMouseSequence() {
        std::string seq;
        timeout(20);
        for (int i = 0; i < 32; i++) {
            int ch = getch();
            if (ch == ERR) {
                break;
            }
            seq.push_back((char)ch);
            if (ch == 'M' || ch == 'm') {
                break;
            }
        }
        timeout(-1);

        if (seq.size() >= 6 && seq[0] == '[' && seq[1] == '<') {
            int button = 0;
            int x = 0;
            int y = 0;
            char end = '\0';
            if (std::sscanf(seq.c_str(), "[<%d;%d;%d%c", &button, &x, &y, &end) == 4) {
                handleDecodedMouse(button, x - 1, y - 1);
                return true;
            }
        }

        if (seq.size() >= 5 && seq[0] == '[' && seq[1] == 'M') {
            int button = (unsigned char)seq[2] - 32;
            int x = (unsigned char)seq[3] - 33;
            int y = (unsigned char)seq[4] - 33;
            handleDecodedMouse(button, x, y);
            return true;
        }

        return false;
    }

    void handleKey(int key) {
        if (key == CTRL_KEY('q')) {
            running = false;
            return;
        }
        if (key == CTRL_KEY('s')) {
            save();
            return;
        }
        if (key == CTRL_KEY('r')) {
            revert();
            return;
        }
        if (key == KEY_MOUSE) {
            handleMouse();
            return;
        }
        if (key == 27 && handleEscapeMouseSequence()) {
            return;
        }
        if (key == KEY_F(1)) {
            activePane = PANE_EDITOR;
            message = "Editor focused.";
            return;
        }
        if (key == KEY_F(2)) {
            activePane = PANE_TOKENS;
            message = "Tokens pane focused. Use [], arrows, PageUp/PageDown.";
            return;
        }
        if (key == KEY_F(3)) {
            activePane = PANE_PARSER;
            message = "Parser pane focused. Use [], arrows, PageUp/PageDown.";
            return;
        }
        if (key == '[') {
            scrollActivePane(-3);
            return;
        }
        if (key == ']') {
            scrollActivePane(3);
            return;
        }

        if (activePane != PANE_EDITOR) {
            if (key == KEY_UP) {
                scrollActivePane(-1);
                return;
            }
            if (key == KEY_DOWN) {
                scrollActivePane(1);
                return;
            }
            if (key == KEY_PPAGE) {
                scrollActivePane(-visibleRowsForPane(activePane));
                return;
            }
            if (key == KEY_NPAGE) {
                scrollActivePane(visibleRowsForPane(activePane));
                return;
            }
            if (key == KEY_HOME) {
                scrollActivePane(activePane == PANE_TOKENS ? -tokenScroll : -parserScroll);
                return;
            }
            if (key == KEY_END) {
                int visibleRows = visibleRowsForPane(activePane);
                int maxScroll = activePane == PANE_TOKENS
                    ? maxScrollForRows(analysis.tokenLines, visibleRows)
                    : maxScrollForRows(analysis.syntaxLines, visibleRows);
                scrollActivePane(maxScroll);
                return;
            }
        }

        if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN ||
            key == KEY_HOME || key == KEY_END || key == KEY_PPAGE || key == KEY_NPAGE) {
            moveCursor(key);
            return;
        }

        if (key == KEY_BACKSPACE || key == 127 || key == 8) {
            backspace();
            return;
        }
        if (key == KEY_DC) {
            deleteChar();
            return;
        }
        if (key == '\n' || key == '\r') {
            insertNewline();
            return;
        }
        if (key == '\t') {
            insertChar(' ');
            insertChar(' ');
            return;
        }
        if (key >= 32 && key <= 126) {
            insertChar(key);
        }
    }

    void drawEditorLine(int screenY, int x, int width, int lineIndex) {
        drawFill(screenY, x, width, COLOR_NORMAL_TEXT);

        if (lineIndex >= (int)lines.size()) {
            return;
        }

        const int lineNoWidth = 5;
        std::ostringstream no;
        no.width(4);
        no << (lineIndex + 1) << " ";
        drawText(screenY, x, std::min(lineNoWidth, width), no.str(), COLOR_LINE_NO_TEXT);

        int textX = x + lineNoWidth;
        int textWidth = width - lineNoWidth;
        if (textWidth <= 0) {
            return;
        }

        const std::string& line = lines[lineIndex];
        std::vector<int> colors(line.size(), COLOR_NORMAL_TEXT);
        std::vector<bool> selected(line.size(), false);
        std::vector<bool> errorChars(line.size(), false);
        std::vector<int> virtualErrorCols;
        int currentToken = selectedTokenIndex();
        for (size_t i = 0; i < analysis.spans.size(); i++) {
            const TokenSpan& span = analysis.spans[i];
            if (span.line != lineIndex + 1) {
                continue;
            }
            int begin = std::max(0, span.column - 1);
            int end = std::min((int)line.size(), begin + span.length);
            for (int pos = begin; pos < end; pos++) {
                colors[pos] = colorForToken(span.type);
                if ((int)i == currentToken) {
                    selected[pos] = true;
                }
            }
        }
        for (size_t i = 0; i < analysis.errors.size(); i++) {
            const ErrorSpan& error = analysis.errors[i];
            if (error.line != lineIndex + 1) {
                continue;
            }

            int begin = std::max(0, error.column - 1);
            int end = std::min((int)line.size(), begin + std::max(1, error.length));
            if (begin >= (int)line.size()) {
                virtualErrorCols.push_back(begin);
                continue;
            }

            for (int pos = begin; pos < end; pos++) {
                colors[pos] = COLOR_ERROR_TEXT;
                errorChars[pos] = true;
            }
        }

        for (int i = 0; i < textWidth; i++) {
            int sourceCol = leftCol + i;
            if (sourceCol >= (int)line.size()) {
                break;
            }

            char ch = line[sourceCol];
            if (ch == '\t') {
                ch = ' ';
            }
            attron(COLOR_PAIR(colors[sourceCol]));
            if (errorChars[sourceCol]) {
                attron(A_UNDERLINE);
                attron(A_BOLD);
            }
            if (selected[sourceCol]) {
                attron(A_REVERSE);
            }
            mvaddch(screenY, textX + i, ch);
            if (selected[sourceCol]) {
                attroff(A_REVERSE);
            }
            if (errorChars[sourceCol]) {
                attroff(A_BOLD);
                attroff(A_UNDERLINE);
            }
            attroff(COLOR_PAIR(colors[sourceCol]));
        }

        for (size_t i = 0; i < virtualErrorCols.size(); i++) {
            int sourceCol = virtualErrorCols[i];
            if (sourceCol < leftCol || sourceCol >= leftCol + textWidth) {
                continue;
            }
            attron(COLOR_PAIR(COLOR_ERROR_TEXT));
            attron(A_BOLD);
            attron(A_UNDERLINE);
            mvaddch(screenY, textX + sourceCol - leftCol, '~');
            attroff(A_UNDERLINE);
            attroff(A_BOLD);
            attroff(COLOR_PAIR(COLOR_ERROR_TEXT));
        }
    }

    void drawListPane(
        int y,
        int x,
        int height,
        int width,
        const std::string& title,
        const std::vector<std::string>& rows,
        bool ok,
        int scroll,
        bool focused,
        int selectedToken
    ) {
        if (height <= 0 || width <= 0) {
            return;
        }

        int visibleRows = std::max(0, height - 1);
        int maxScroll = maxScrollForRows(rows, visibleRows);
        scroll = clampInt(scroll, 0, maxScroll);

        std::ostringstream header;
        header << (focused ? "> " : "  ") << title;
        if (maxScroll > 0) {
            header << " [" << (scroll + 1) << "/" << (maxScroll + 1) << "]";
        }
        drawText(y, x, width, header.str(), COLOR_HEADER_TEXT, true);

        int rowCount = height - 1;
        for (int i = 0; i < rowCount; i++) {
            int screenY = y + 1 + i;
            drawFill(screenY, x, width, COLOR_NORMAL_TEXT);
            int rowIndex = scroll + i;
            if (rowIndex >= (int)rows.size()) {
                continue;
            }

            int color = ok ? COLOR_NORMAL_TEXT : COLOR_ERROR_TEXT;
            bool selected = false;
            if (&rows == &analysis.tokenLines && selectedToken >= 0 && selectedToken < (int)analysis.spans.size()) {
                selected = rowIndex == analysis.spans[selectedToken].tokenLine;
            } else if (&rows == &analysis.syntaxLines) {
                selected = syntaxRowMatchesToken(rows[rowIndex], selectedToken);
            }
            drawText(screenY, x, width, rows[rowIndex], color, selected, selected);
        }
    }

    void draw() {
        erase();

        if (LINES < 10 || COLS < 40) {
            drawText(0, 0, COLS, "Terminal is too small. Please enlarge it.", COLOR_ERROR_TEXT, true);
            refresh();
            return;
        }

        int statusY = LINES - 1;
        int contentH = LINES - 1;
        int leftW = COLS / 2;
        int sepX = leftW;
        int rightX = sepX + 1;
        int rightW = COLS - rightX;
        int topPaneH = contentH / 2;
        int bottomPaneH = contentH - topPaneH;
        int editorRows = contentH - 1;
        int textWidth = leftW - 5;

        ensureCursorVisible(editorRows, textWidth);
        keepSelectedTokenVisible();
        int selectedToken = selectedTokenIndex();

        std::string title = " C-- Editor: " + filePath;
        if (modified) {
            title += " *";
        }
        drawText(0, 0, leftW, title, COLOR_HEADER_TEXT, true);

        for (int y = 0; y < contentH; y++) {
            mvaddch(y, sepX, ACS_VLINE);
        }

        for (int i = 0; i < editorRows; i++) {
            drawEditorLine(1 + i, 0, leftW, topLine + i);
        }

        clampPanelScrolls();

        std::string lexTitle = analysis.lexOk ? "Tokens: OK" : "Tokens: ERROR";
        drawListPane(0, rightX, topPaneH, rightW, lexTitle, analysis.tokenLines, analysis.lexOk, tokenScroll, activePane == PANE_TOKENS, selectedToken);

        std::string parseTitle = analysis.parseOk ? "Parser: OK" : "Parser: ERROR";
        drawListPane(topPaneH, rightX, bottomPaneH, rightW, parseTitle, analysis.syntaxLines, analysis.parseOk, parserScroll, activePane == PANE_PARSER, selectedToken);

        std::ostringstream status;
        int currentError = errorIndexAtCursor();
        status << "Ln " << (cursorLine + 1) << ", Col " << (cursorCol + 1);
        if (activePane == PANE_TOKENS) {
            status << " | Focus: Tokens";
        } else if (activePane == PANE_PARSER) {
            status << " | Focus: Parser";
        } else {
            status << " | Focus: Editor";
        }
        if (selectedToken >= 0 && selectedToken < (int)analysis.spans.size()) {
            status << " | Token: " << analysis.spans[selectedToken].lexeme
                   << "/" << analysis.spans[selectedToken].grammar;
        }
        if (currentError >= 0 && currentError < (int)analysis.errors.size()) {
            status << " | ERROR: " << analysis.errors[currentError].message;
        } else if (!analysis.lexOk && !analysis.lexError.empty()) {
            status << " | Lexical errors: " << analysis.errors.size();
        } else if (!analysis.parseOk && !analysis.parseError.empty()) {
            status << " | Syntax error";
        } else {
            status << " | " << message;
        }
        drawText(statusY, 0, COLS, status.str(), COLOR_STATUS_TEXT, true);

        int cursorY = 1 + cursorLine - topLine;
        int cursorX = 5 + cursorCol - leftCol;
        if (cursorY >= 1 && cursorY < contentH && cursorX >= 5 && cursorX < leftW) {
            move(cursorY, cursorX);
        }

        refresh();
    }
};

} // namespace

int main(int argc, char** argv) {
    std::string path = "untitled.sy";
    if (argc >= 2) {
        path = argv[1];
    }

    std::setlocale(LC_ALL, "");
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);
    enableTerminalMouse();
    initColors();
    curs_set(1);

    Editor editor(path);
    editor.run();

    disableTerminalMouse();
    endwin();
    return 0;
}

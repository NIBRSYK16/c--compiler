#include "c--/lexer/lexer.h"

#include "c--/lexer/automata.h"

#include <cctype>
#include <map>
#include <sstream>
#include <string>

namespace cminus {

namespace {

bool isBlank(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool isIdentifierContinue(char ch) {
    unsigned char value = (unsigned char)ch;
    return std::isalnum(value) || ch == '_';
}

int sameLineLength(const std::string& source, int begin, int fallback) {
    int length = 0;
    while (begin + length < (int)source.size() && source[begin + length] != '\n') {
        length++;
    }
    return length > 0 ? length : fallback;
}

void updatePosition(const std::string& source, int begin, int end, int& line, int& column) {
    for (int i = begin; i < end; i++) {
        if (source[i] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
}

void addLexDiagnostic(LexResult& result,
                      std::ostringstream& errors,
                      const std::string& code,
                      const std::string& message,
                      int line,
                      int column,
                      int length,
                      const std::string& hint = "");

std::string toLower(const std::string& text) {
    std::string result = text;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] = (char)std::tolower((unsigned char)result[i]);
    }
    return result;
}

bool startsWith(const std::string& source, int pos, const char* text) {
    int offset = 0;
    while (text[offset] != '\0') {
        if (pos + offset >= (int)source.size() || source[pos + offset] != text[offset]) {
            return false;
        }
        offset++;
    }
    return true;
}

bool skipLineComment(const std::string& source, int& pos, int& line, int& column) {
    if (!startsWith(source, pos, "//")) {
        return false;
    }

    int begin = pos;
    pos += 2;
    while (pos < (int)source.size() && source[pos] != '\n') {
        pos++;
    }

    updatePosition(source, begin, pos, line, column);
    return true;
}

bool skipBlockComment(const std::string& source,
                      int& pos,
                      int& line,
                      int& column,
                      LexResult& result,
                      std::ostringstream& errors,
                      bool& hasError) {
    if (!startsWith(source, pos, "/*")) {
        return false;
    }

    int begin = pos;
    int beginLine = line;
    int beginColumn = column;
    pos += 2;

    while (pos + 1 < (int)source.size() && !startsWith(source, pos, "*/")) {
        pos++;
    }

    if (pos + 1 >= (int)source.size()) {
        updatePosition(source, begin, (int)source.size(), line, column);
        pos = (int)source.size();
        hasError = true;
        addLexDiagnostic(result,
                         errors,
                         "LEX_UNTERMINATED_COMMENT",
                         "unterminated block comment",
                         beginLine,
                         beginColumn,
                         2,
                         "add a closing */ before the end of file");
        return true;
    }

    pos += 2;
    updatePosition(source, begin, pos, line, column);
    return true;
}

bool isMalformedNumberSuffix(const std::string& source, int acceptPos) {
    if (acceptPos >= (int)source.size()) {
        return false;
    }

    char next = source[acceptPos];
    if (isIdentifierContinue(next)) {
        return true;
    }

    return next == '.';
}

int consumeMalformedNumber(const std::string& source, int pos, int acceptPos) {
    int cursor = acceptPos;
    if (cursor < (int)source.size() && source[cursor] == '.') {
        cursor++;
    }

    while (cursor < (int)source.size() && isIdentifierContinue(source[cursor])) {
        cursor++;
    }

    return cursor > pos ? cursor : acceptPos;
}

TokenType tokenTypeFromRule(const std::string& ruleName) {
    if (ruleName == "KW") {
        return TokenType::Keyword;
    }
    if (ruleName == "IDN") {
        return TokenType::Identifier;
    }
    if (ruleName == "INT") {
        return TokenType::Integer;
    }
    if (ruleName == "FLOAT") {
        return TokenType::Float;
    }
    if (ruleName == "OP") {
        return TokenType::Operator;
    }
    if (ruleName == "SE") {
        return TokenType::Separator;
    }
    return TokenType::Unknown;
}

std::string grammarName(const std::string& lexeme, const TokenRule& rule) {
    if (rule.name == "IDN") {
        return "Ident";
    }
    if (rule.name == "INT") {
        return "IntConst";
    }
    if (rule.name == "FLOAT") {
        return "floatConst";
    }
    if (rule.name == "KW") {
        return toLower(lexeme);
    }
    return lexeme;
}

std::string tokenAttr(const std::string& lexeme, const TokenRule& rule) {
    if (rule.name == "IDN" || rule.name == "INT" || rule.name == "FLOAT") {
        return lexeme;
    }
    return rule.attr;
}

std::string describeChar(char ch) {
    if (ch == '\n') {
        return "\\n";
    }
    if (ch == '\t') {
        return "\\t";
    }
    if (ch == '\r') {
        return "\\r";
    }

    std::string result;
    result.push_back(ch);
    return result;
}

void addLexDiagnostic(LexResult& result,
                      std::ostringstream& errors,
                      const std::string& code,
                      const std::string& message,
                      int line,
                      int column,
                      int length,
                      const std::string& hint) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Error;
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.range.line = line;
    diagnostic.range.column = column;
    diagnostic.range.length = length;
    diagnostic.hint = hint;
    result.diagnostics.push_back(diagnostic);
    errors << renderDiagnostic(diagnostic) << '\n';
}

Token makeToken(const std::string& lexeme, const TokenRule& rule, int line, int column) {
    Token token;
    token.lexeme = lexeme;
    token.type = tokenTypeFromRule(rule.name);
    token.grammar = grammarName(lexeme, rule);
    token.attr = tokenAttr(lexeme, rule);
    token.line = line;
    token.column = column;
    return token;
}

Token makeEOFToken(int line, int column) {
    Token token;
    token.lexeme = "EOF";
    token.type = TokenType::EndOfFile;
    token.grammar = "EOF";
    token.attr = "EOF";
    token.line = line;
    token.column = column;
    return token;
}

} // namespace

LexResult Lexer::tokenize(const std::string& source) {
    LexResult result;

    const DFA& dfa = getMinimizedDFA();
    const std::vector<TokenRule>& rules = getTokenRules();

    int line = 1;
    int column = 1;
    int pos = 0;
    int length = (int)source.size();
    std::ostringstream errors;
    bool hasError = false;

    while (pos < length) {
        char current = source[pos];

        if (isBlank(current)) {
            updatePosition(source, pos, pos + 1, line, column);
            pos++;
            continue;
        }

        if (skipLineComment(source, pos, line, column)) {
            continue;
        }

        if (skipBlockComment(source, pos, line, column, result, errors, hasError)) {
            continue;
        }

        int state = dfa.start;
        int cursor = pos;
        int lastAcceptRule = -1;
        int lastAcceptPos = -1;

        while (cursor < length) {
            unsigned char ch = (unsigned char)source[cursor];
            if (ch >= 128) {
                break;
            }

            std::map<int, int>::const_iterator next = dfa.states[state].trans.find(ch);
            if (next == dfa.states[state].trans.end()) {
                break;
            }

            state = next->second;
            cursor++;

            if (dfa.states[state].acceptRule >= 0) {
                lastAcceptRule = dfa.states[state].acceptRule;
                lastAcceptPos = cursor;
            }
        }

        if (lastAcceptRule >= 0) {
            std::string lexeme = source.substr(pos, lastAcceptPos - pos);
            const TokenRule& rule = rules[lastAcceptRule];
            if ((rule.name == "INT" || rule.name == "FLOAT") &&
                isMalformedNumberSuffix(source, lastAcceptPos)) {
                int malformedEnd = consumeMalformedNumber(source, pos, lastAcceptPos);
                std::string malformed = source.substr(pos, malformedEnd - pos);
                hasError = true;
                addLexDiagnostic(result,
                                 errors,
                                 "LEX_MALFORMED_NUMBER",
                                 "malformed numeric literal '" + malformed + "'",
                                 line,
                                 column,
                                 (int)malformed.size(),
                                 "split the identifier from the number or write a complete float literal");
                updatePosition(source, pos, malformedEnd, line, column);
                pos = malformedEnd;
                continue;
            }
            result.tokens.push_back(makeToken(lexeme, rule, line, column));
            updatePosition(source, pos, lastAcceptPos, line, column);
            pos = lastAcceptPos;
            continue;
        }

        hasError = true;
        addLexDiagnostic(result,
                         errors,
                         "LEX_UNEXPECTED_CHAR",
                         "unexpected character '" + describeChar(source[pos]) + "'",
                         line,
                         column,
                         sameLineLength(source, pos, 1),
                         "remove this character or replace it with a valid C-- token");
        updatePosition(source, pos, pos + 1, line, column);
        pos++;
    }

    result.tokens.push_back(makeEOFToken(line, column));
    result.success = !hasError;
    result.errorMessage = errors.str();
    return result;
}

} // namespace cminus

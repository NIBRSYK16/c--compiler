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

std::string toLower(const std::string& text) {
    std::string result = text;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] = (char)std::tolower((unsigned char)result[i]);
    }
    return result;
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

        if (current == '/' && pos + 1 < length && source[pos + 1] == '/') {
            int begin = pos;
            pos += 2;
            while (pos < length && source[pos] != '\n') {
                pos++;
            }
            updatePosition(source, begin, pos, line, column);
            continue;
        }

        if (current == '/' && pos + 1 < length && source[pos + 1] == '*') {
            int begin = pos;
            int commentLine = line;
            int commentColumn = column;
            bool closed = false;

            pos += 2;
            while (pos + 1 < length) {
                if (source[pos] == '*' && source[pos + 1] == '/') {
                    pos += 2;
                    closed = true;
                    break;
                }
                pos++;
            }

            if (!closed) {
                hasError = true;
                errors << "Lexical error at line " << commentLine
                       << ", column " << commentColumn
                       << ": unterminated block comment.\n";
                updatePosition(source, begin, length, line, column);
                pos = length;
                continue;
            }

            updatePosition(source, begin, pos, line, column);
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
            result.tokens.push_back(makeToken(lexeme, rules[lastAcceptRule], line, column));
            updatePosition(source, pos, lastAcceptPos, line, column);
            pos = lastAcceptPos;
            continue;
        }

        hasError = true;
        errors << "Lexical error at line " << line
               << ", column " << column
               << ": unexpected character '" << describeChar(source[pos]) << "'.\n";
        updatePosition(source, pos, pos + 1, line, column);
        pos++;
    }

    result.tokens.push_back(makeEOFToken(line, column));
    result.success = !hasError;
    result.errorMessage = errors.str();
    return result;
}

} // namespace cminus

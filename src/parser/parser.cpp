#include "c--/parser/parser.h"

#include <sstream>
#include <string>
#include <vector>

namespace cminus {

namespace {

struct DelimiterFrame {
    std::string symbol;
    Token token;
};

Diagnostic makeSyntaxDiagnostic(const std::string& code,
                                DiagnosticSeverity severity,
                                const std::string& message,
                                const Token& token,
                                const std::string& hint = "") {
    Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.range.line = token.line;
    diagnostic.range.column = token.column;
    diagnostic.range.length = token.lexeme.empty() ? 1 : (int)token.lexeme.size();
    diagnostic.hint = hint;
    return diagnostic;
}

bool isValueToken(const Token& token) {
    return token.grammar == "Ident" ||
           token.grammar == "IntConst" ||
           token.grammar == "floatConst" ||
           token.grammar == ")";
}

bool isExpressionStarter(const Token& token) {
    return token.grammar == "Ident" ||
           token.grammar == "IntConst" ||
           token.grammar == "floatConst" ||
           token.grammar == "(" ||
           token.grammar == "+" ||
           token.grammar == "-" ||
           token.grammar == "!";
}

bool isBinaryOperator(const Token& token) {
    return token.grammar == "+" ||
           token.grammar == "-" ||
           token.grammar == "*" ||
           token.grammar == "/" ||
           token.grammar == "%" ||
           token.grammar == "==" ||
           token.grammar == "!=" ||
           token.grammar == "<" ||
           token.grammar == "<=" ||
           token.grammar == ">" ||
           token.grammar == ">=" ||
           token.grammar == "&&" ||
           token.grammar == "||" ||
           token.grammar == "=";
}

bool isTerminator(const Token& token) {
    return token.grammar == ";" ||
           token.grammar == "," ||
           token.grammar == ")" ||
           token.grammar == "}" ||
           token.grammar == "EOF";
}

bool isOpenDelimiter(const Token& token) {
    return token.grammar == "(" || token.grammar == "{";
}

std::string matchingOpen(const std::string& close) {
    if (close == ")") {
        return "(";
    }
    if (close == "}") {
        return "{";
    }
    return "";
}

void addDiagnostic(TokenStreamSummary& summary, const Diagnostic& diagnostic) {
    summary.diagnostics.push_back(diagnostic);
    if (diagnostic.severity == DiagnosticSeverity::Error) {
        summary.success = false;
    }
}

} // namespace

TokenStreamSummary Parser::precheckTokenStream(const std::vector<Token>& tokens) const {
    TokenStreamSummary summary;
    summary.tokenCount = (int)tokens.size();

    if (tokens.empty()) {
        Token synthetic;
        synthetic.lexeme = "";
        addDiagnostic(summary,
                      makeSyntaxDiagnostic("SYN_EMPTY_TOKEN_STREAM",
                                           DiagnosticSeverity::Error,
                                           "empty token stream",
                                           synthetic,
                                           "run the lexer first and make sure EOF is emitted"));
        return summary;
    }

    std::vector<DelimiterFrame> delimiters;
    int parenDepth = 0;
    int braceDepth = 0;
    int pendingIf = 0;

    for (size_t i = 0; i < tokens.size(); i++) {
        const Token& token = tokens[i];
        if (token.grammar == "EOF") {
            summary.eofCount++;
        }

        if (isOpenDelimiter(token)) {
            DelimiterFrame frame;
            frame.symbol = token.grammar;
            frame.token = token;
            delimiters.push_back(frame);

            if (token.grammar == "(") {
                parenDepth++;
                if (parenDepth > summary.maxParenDepth) {
                    summary.maxParenDepth = parenDepth;
                }
            } else {
                braceDepth++;
                if (braceDepth > summary.maxBraceDepth) {
                    summary.maxBraceDepth = braceDepth;
                }
            }
        } else if (token.grammar == ")" || token.grammar == "}") {
            std::string expectedOpen = matchingOpen(token.grammar);
            if (delimiters.empty() || delimiters.back().symbol != expectedOpen) {
                addDiagnostic(summary,
                              makeSyntaxDiagnostic("SYN_UNMATCHED_DELIMITER",
                                                   DiagnosticSeverity::Error,
                                                   "unmatched closing delimiter '" + token.lexeme + "'",
                                                   token,
                                                   "check whether an opening '" + expectedOpen + "' is missing before this token"));
            } else {
                delimiters.pop_back();
            }

            if (token.grammar == ")" && parenDepth > 0) {
                parenDepth--;
            }
            if (token.grammar == "}" && braceDepth > 0) {
                braceDepth--;
            }
        }

        if (token.grammar == "if") {
            pendingIf++;
        } else if (token.grammar == "else") {
            if (pendingIf == 0) {
                addDiagnostic(summary,
                              makeSyntaxDiagnostic("SYN_ELSE_WITHOUT_IF",
                                                   DiagnosticSeverity::Error,
                                                   "else appears without a visible matching if",
                                                   token,
                                                   "move this else after an if statement or remove it"));
            } else {
                pendingIf--;
            }
        }

        if (i + 1 < tokens.size()) {
            const Token& next = tokens[i + 1];
            if (isBinaryOperator(token) && isTerminator(next)) {
                addDiagnostic(summary,
                              makeSyntaxDiagnostic("SYN_MISSING_OPERAND",
                                                   DiagnosticSeverity::Error,
                                                   "operator '" + token.lexeme + "' is missing a right operand",
                                                   token,
                                                   "insert an expression before '" + next.lexeme + "'"));
            }

            if (isValueToken(token) && isExpressionStarter(next) && next.grammar != "(") {
                addDiagnostic(summary,
                              makeSyntaxDiagnostic("SYN_ADJACENT_EXPRESSIONS",
                                                   DiagnosticSeverity::Warning,
                                                   "two expression tokens appear next to each other",
                                                   next,
                                                   "insert an operator, comma, semicolon, or parentheses if this is intentional"));
            }

            if ((token.grammar == "return" || token.grammar == "if") && isTerminator(next) && next.grammar != "(") {
                addDiagnostic(summary,
                              makeSyntaxDiagnostic("SYN_SUSPICIOUS_EMPTY_CONSTRUCT",
                                                   DiagnosticSeverity::Warning,
                                                   "construct '" + token.lexeme + "' has no following expression",
                                                   token,
                                                   "confirm this matches the C-- grammar before feeding it to SLR parsing"));
            }
        }
    }

    for (size_t i = 0; i < delimiters.size(); i++) {
        const DelimiterFrame& frame = delimiters[i];
        std::string close = frame.symbol == "(" ? ")" : "}";
        addDiagnostic(summary,
                      makeSyntaxDiagnostic("SYN_UNCLOSED_DELIMITER",
                                           DiagnosticSeverity::Error,
                                           "unclosed delimiter '" + frame.symbol + "'",
                                           frame.token,
                                           "add a matching '" + close + "' later in the same syntactic region"));
    }

    if (summary.eofCount == 0) {
        addDiagnostic(summary,
                      makeSyntaxDiagnostic("SYN_MISSING_EOF",
                                           DiagnosticSeverity::Error,
                                           "token stream has no EOF marker",
                                           tokens.back(),
                                           "append the EOF token produced by the lexer"));
    } else if (summary.eofCount > 1) {
        addDiagnostic(summary,
                      makeSyntaxDiagnostic("SYN_DUPLICATE_EOF",
                                           DiagnosticSeverity::Error,
                                           "token stream contains more than one EOF marker",
                                           tokens.back(),
                                           "keep only the final EOF token"));
    } else if (tokens.back().grammar != "EOF") {
        addDiagnostic(summary,
                      makeSyntaxDiagnostic("SYN_TRAILING_AFTER_EOF",
                                           DiagnosticSeverity::Error,
                                           "EOF marker is not the final token",
                                           tokens.back(),
                                           "move EOF to the end or discard trailing tokens"));
    }

    return summary;
}

ParseResult Parser::parse(const std::vector<Token>& tokens) {
    ParseResult result;
    TokenStreamSummary summary = precheckTokenStream(tokens);
    result.diagnostics = summary.diagnostics;

    for (size_t i = 0; i < summary.diagnostics.size(); i++) {
        result.reduceLogs.push_back(renderDiagnostic(summary.diagnostics[i]));
    }

    if (!summary.success) {
        result.success = false;
        result.errorMessage = renderDiagnostics(summary.diagnostics);
        return result;
    }

    result.success = false;
    std::ostringstream message;
    message << "Parser SLR core is not implemented yet. "
            << "Token precheck passed: tokens=" << summary.tokenCount
            << ", max_paren_depth=" << summary.maxParenDepth
            << ", max_brace_depth=" << summary.maxBraceDepth << ".";
    result.errorMessage = message.str();
    return result;
}

} // namespace cminus

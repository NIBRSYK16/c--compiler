#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace cminus {

enum class DiagnosticSeverity {
    Note,
    Warning,
    Error
};

struct SourceRange {
    int line = 1;
    int column = 1;
    int length = 1;
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string code;
    std::string message;
    SourceRange range;
    std::string hint;
};

inline std::string diagnosticSeverityName(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Note:
            return "note";
        case DiagnosticSeverity::Warning:
            return "warning";
        case DiagnosticSeverity::Error:
            return "error";
    }
    return "error";
}

inline std::string renderDiagnostic(const Diagnostic& diagnostic) {
    std::ostringstream output;
    output << diagnosticSeverityName(diagnostic.severity);
    if (!diagnostic.code.empty()) {
        output << "[" << diagnostic.code << "]";
    }
    output << " at line " << diagnostic.range.line
           << ", column " << diagnostic.range.column
           << ": " << diagnostic.message;
    if (!diagnostic.hint.empty()) {
        output << "\n  hint: " << diagnostic.hint;
    }
    return output.str();
}

inline std::string renderDiagnostics(const std::vector<Diagnostic>& diagnostics) {
    std::ostringstream output;
    for (size_t i = 0; i < diagnostics.size(); i++) {
        output << renderDiagnostic(diagnostics[i]) << '\n';
    }
    return output.str();
}

inline std::string escapeJsonString(const std::string& text) {
    std::ostringstream output;
    for (size_t i = 0; i < text.size(); i++) {
        char ch = text[i];
        switch (ch) {
            case '\\':
                output << "\\\\";
                break;
            case '"':
                output << "\\\"";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                output << ch;
                break;
        }
    }
    return output.str();
}

inline std::string renderDiagnosticJson(const Diagnostic& diagnostic) {
    std::ostringstream output;
    output << "{"
           << "\"severity\":\"" << diagnosticSeverityName(diagnostic.severity) << "\","
           << "\"code\":\"" << escapeJsonString(diagnostic.code) << "\","
           << "\"message\":\"" << escapeJsonString(diagnostic.message) << "\","
           << "\"line\":" << diagnostic.range.line << ","
           << "\"column\":" << diagnostic.range.column << ","
           << "\"length\":" << diagnostic.range.length << ","
           << "\"hint\":\"" << escapeJsonString(diagnostic.hint) << "\""
           << "}";
    return output.str();
}

inline std::string renderDiagnosticsJsonLines(const std::vector<Diagnostic>& diagnostics) {
    std::ostringstream output;
    for (size_t i = 0; i < diagnostics.size(); i++) {
        output << renderDiagnosticJson(diagnostics[i]) << '\n';
    }
    return output.str();
}

} // namespace cminus

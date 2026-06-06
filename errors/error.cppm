module;

#include <string>
#include <string_view>
#include <vector>
#include <cstdio>
#include <algorithm>

export module error;

// -------------------------------------------------------
// ErrorStage: which compiler phase produced the error
// -------------------------------------------------------
export enum class ErrorStage {
    Lexer,
    Parser,
    Resolver,
    TypeChecker,
    Codegen
};

// -------------------------------------------------------
// Diagnostic: one error record
// -------------------------------------------------------
export struct Diagnostic {
    ErrorStage  stage;
    int         line;
    int         col;       // 0 if unknown
    std::string where;     // token text or short context
    std::string message;
};

// -------------------------------------------------------
// Diagnostics: the single shared error sink
// Passed by pointer into every phase.
// -------------------------------------------------------
export class Diagnostics {
public:
    // Record one error. Duplicate suppression: if the same
    // (stage, line, message) was already recorded, drop it.
    void error(ErrorStage stage,
               int line,
               std::string where,
               std::string message,
               int col = 0)
    {
        // Deduplicate: same line + message in same stage only once
        for (auto& d : _diags) {
            if (d.stage == stage &&
                d.line  == line  &&
                d.message == message) return;
        }
        _diags.push_back({ stage, line, col, std::move(where), std::move(message) });
        _had_error = true;
    }

    // True if any error was recorded in ANY stage
    bool had_error() const { return _had_error; }

    // True if any error was recorded in a specific stage
    bool had_error_in(ErrorStage stage) const {
        for (auto& d : _diags)
            if (d.stage == stage) return true;
        return false;
    }

    // Print all recorded diagnostics, grouped by stage in order
    // Format: <line>:<col>: [stage] error: <message> (<where>)
    void print_all(std::string_view filename = "") const {
        static const char* stage_names[] = {
            "lexer", "parser", "resolver", "type", "codegen"
        };
        for (auto& d : _diags) {
            const char* stage = stage_names[(int)d.stage];
            if (!filename.empty())
                std::fprintf(stderr, "%s:", std::string(filename).c_str());
            if (d.col > 0)
                std::fprintf(stderr, "%d:%d: ", d.line, d.col);
            else
                std::fprintf(stderr, "%d: ", d.line);
            std::fprintf(stderr, "[%s] error: %s",
                         stage, d.message.c_str());
            if (!d.where.empty())
                std::fprintf(stderr, " ('%s')", d.where.c_str());
            std::fprintf(stderr, "\n");
        }
    }

    // How many errors total
    size_t count() const { return _diags.size(); }

    void clear() {
        _diags.clear();
        _had_error = false;
    }

private:
    std::vector<Diagnostic> _diags;
    bool _had_error = false;
};

// -------------------------------------------------------
// ParseError: thrown only to unwind the parser stack.
// NOT used for control flow visible outside the parser --
// the parser catches it internally and tries to synchronize.
// main.cpp never sees this thrown.
// -------------------------------------------------------
export struct ParseError : public std::exception {
    const char* what() const noexcept override { return "parse error"; }
};
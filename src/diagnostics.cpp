#include "zenasm/diagnostics.hpp"

#include <sstream>

namespace zenasm {

Diagnostics::Diagnostics(const SourceFile& source)
    : source_(source) {}

Diagnostic& Diagnostics::error(const SourceSpan& span, std::string message) {
    return add(DiagnosticLevel::Error, span, std::move(message));
}

Diagnostic& Diagnostics::warning(const SourceSpan& span, std::string message) {
    return add(DiagnosticLevel::Warning, span, std::move(message));
}

bool Diagnostics::hasErrors() const noexcept {
    for (const auto& entry : entries_) {
        if (entry.level == DiagnosticLevel::Error) {
            return true;
        }
    }
    return false;
}

const std::vector<Diagnostic>& Diagnostics::entries() const noexcept {
    return entries_;
}

std::string Diagnostics::render() const {
    std::ostringstream output;
    for (const auto& entry : entries_) {
        output << renderOne(entry);
        if (&entry != &entries_.back()) {
            output << '\n';
        }
    }
    return output.str();
}

Diagnostic& Diagnostics::add(const DiagnosticLevel level, const SourceSpan& span, std::string message) {
    entries_.push_back(Diagnostic {
        .level = level,
        .message = std::move(message),
        .span = span,
        .notes = {},
    });
    return entries_.back();
}

std::string Diagnostics::renderOne(const Diagnostic& diagnostic) const {
    std::ostringstream output;
    output << source_.path() << ':' << diagnostic.span.begin.line << ':' << diagnostic.span.begin.column << ": ";
    output << (diagnostic.level == DiagnosticLevel::Error ? "error: " : "warning: ");
    output << diagnostic.message << '\n';

    const auto line = source_.lineText(diagnostic.span.begin.line);
    if (!line.empty()) {
        output << "  |\n";
        output << diagnostic.span.begin.line << " | " << line << '\n';
        output << "  | ";
        for (int column = 1; column < diagnostic.span.begin.column; ++column) {
            output << ' ';
        }
        const int highlight_width = std::max(1, diagnostic.span.end.column - diagnostic.span.begin.column);
        for (int index = 0; index < highlight_width; ++index) {
            output << '^';
        }
        output << '\n';
    }

    for (const auto& note : diagnostic.notes) {
        output << "note";
        if (note.span.valid()) {
            output << " at " << source_.path() << ':' << note.span.begin.line << ':' << note.span.begin.column;
        }
        output << ": " << note.message << '\n';
    }

    return output.str();
}

}  // namespace zenasm

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "zenasm/source.hpp"

namespace zenasm {

enum class DiagnosticLevel {
    Error,
    Warning,
};

struct DiagnosticNote {
    SourceSpan span {};
    std::string message {};
};

struct Diagnostic {
    DiagnosticLevel level = DiagnosticLevel::Error;
    std::string message {};
    SourceSpan span {};
    std::vector<DiagnosticNote> notes {};
};

class Diagnostics {
  public:
    explicit Diagnostics(const SourceFile& source);

    Diagnostic& error(const SourceSpan& span, std::string message);
    Diagnostic& warning(const SourceSpan& span, std::string message);

    [[nodiscard]] bool hasErrors() const noexcept;
    [[nodiscard]] const std::vector<Diagnostic>& entries() const noexcept;
    [[nodiscard]] std::string render() const;

  private:
    Diagnostic& add(DiagnosticLevel level, const SourceSpan& span, std::string message);
    [[nodiscard]] std::string renderOne(const Diagnostic& diagnostic) const;

    const SourceFile& source_;
    std::vector<Diagnostic> entries_ {};
};

class CompilationError final : public std::runtime_error {
  public:
    explicit CompilationError(const std::string& message)
        : std::runtime_error(message) {}
};

}  // namespace zenasm

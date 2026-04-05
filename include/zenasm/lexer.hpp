#pragma once

#include <vector>

#include "zenasm/diagnostics.hpp"
#include "zenasm/token.hpp"

namespace zenasm {

class Lexer {
  public:
    Lexer(const SourceFile& source, Diagnostics& diagnostics);

    [[nodiscard]] std::vector<Token> tokenize();

  private:
    void lexLine(std::string_view line, int line_number, std::size_t line_offset);
    void emitIndentation(int width, int line_number, std::size_t offset);
    void emit(TokenKind kind, std::string lexeme, SourceSpan span, std::int64_t integer_value = 0);
    [[nodiscard]] SourceSpan makeSpan(std::size_t begin_offset, std::size_t end_offset, int line_number, int begin_column, int end_column) const;
    [[noreturn]] void fail(const SourceSpan& span, const std::string& message);

    const SourceFile& source_;
    Diagnostics& diagnostics_;
    std::vector<Token> tokens_ {};
    std::vector<int> indent_stack_ {0};
};

}  // namespace zenasm

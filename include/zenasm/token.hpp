#pragma once

#include <cstdint>
#include <string>

#include "zenasm/source.hpp"

namespace zenasm {

enum class TokenKind {
    EndOfFile,
    Newline,
    Indent,
    Dedent,
    Identifier,
    Integer,
    String,
    LeftParen,
    RightParen,
    Comma,
    Colon,
    Arrow,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Equal,
    EqualEqual,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    KeywordFn,
    KeywordExtern,
    KeywordLet,
    KeywordReturn,
    KeywordIf,
    KeywordElif,
    KeywordElse,
    KeywordWhile,
    KeywordFor,
    KeywordIn,
    KeywordRepeat,
    KeywordBreak,
    KeywordContinue,
    KeywordTrue,
    KeywordFalse,
    KeywordAnd,
    KeywordOr,
    KeywordNot,
    KeywordAsm,
};

struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string lexeme {};
    std::int64_t integer_value = 0;
    SourceSpan span {};
};

[[nodiscard]] const char* toString(TokenKind kind) noexcept;

}  // namespace zenasm

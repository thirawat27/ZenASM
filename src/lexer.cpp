#include "zenasm/lexer.hpp"

#include <cctype>
#include <unordered_map>

namespace zenasm {

namespace {

const std::unordered_map<std::string, TokenKind> kKeywords = {
    {"fn", TokenKind::KeywordFn},
    {"extern", TokenKind::KeywordExtern},
    {"let", TokenKind::KeywordLet},
    {"return", TokenKind::KeywordReturn},
    {"if", TokenKind::KeywordIf},
    {"elif", TokenKind::KeywordElif},
    {"else", TokenKind::KeywordElse},
    {"while", TokenKind::KeywordWhile},
    {"for", TokenKind::KeywordFor},
    {"in", TokenKind::KeywordIn},
    {"repeat", TokenKind::KeywordRepeat},
    {"break", TokenKind::KeywordBreak},
    {"continue", TokenKind::KeywordContinue},
    {"true", TokenKind::KeywordTrue},
    {"false", TokenKind::KeywordFalse},
    {"and", TokenKind::KeywordAnd},
    {"or", TokenKind::KeywordOr},
    {"not", TokenKind::KeywordNot},
    {"asm", TokenKind::KeywordAsm},
};

}  // namespace

Lexer::Lexer(const SourceFile& source, Diagnostics& diagnostics)
    : source_(source),
      diagnostics_(diagnostics) {}

std::vector<Token> Lexer::tokenize() {
    const auto& line_offsets = source_.lineOffsets();
    for (std::size_t index = 0; index < line_offsets.size(); ++index) {
        const int line_number = static_cast<int>(index + 1);
        const auto line = source_.lineText(line_number);
        const std::size_t line_offset = line_offsets[index];

        std::size_t indentation = 0;
        while (indentation < line.size() && line[indentation] == ' ') {
            ++indentation;
        }
        if (indentation < line.size() && line[indentation] == '\t') {
            fail(makeSpan(line_offset + indentation, line_offset + indentation + 1, line_number, static_cast<int>(indentation + 1), static_cast<int>(indentation + 2)),
                 "tabs are not allowed for indentation");
        }

        if (indentation == line.size() || line[indentation] == '#') {
            continue;
        }

        emitIndentation(static_cast<int>(indentation), line_number, line_offset);
        lexLine(line.substr(indentation), line_number, line_offset + indentation);
        const int end_column = static_cast<int>(line.size() + 1);
        emit(TokenKind::Newline, "\\n", makeSpan(line_offset + line.size(), line_offset + line.size(), line_number, end_column, end_column));
    }

    while (indent_stack_.size() > 1) {
        indent_stack_.pop_back();
        const auto content_size = source_.content().size();
        const int last_line = static_cast<int>(source_.lineOffsets().size());
        emit(TokenKind::Dedent, "<dedent>", makeSpan(content_size, content_size, last_line, 1, 1));
    }

    const auto content_size = source_.content().size();
    const int last_line = std::max(1, static_cast<int>(source_.lineOffsets().size()));
    emit(TokenKind::EndOfFile, "<eof>", makeSpan(content_size, content_size, last_line, 1, 1));
    return tokens_;
}

void Lexer::lexLine(const std::string_view line, const int line_number, const std::size_t line_offset) {
    std::size_t index = 0;
    while (index < line.size()) {
        const char ch = line[index];
        if (ch == ' ') {
            ++index;
            continue;
        }
        if (ch == '#') {
            break;
        }

        const auto begin_offset = line_offset + index;
        const auto begin_column = static_cast<int>(index + 1);

        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            std::size_t end = index + 1;
            while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end])) != 0) {
                ++end;
            }
            const auto lexeme = std::string(line.substr(index, end - index));
            emit(TokenKind::Integer,
                 lexeme,
                 makeSpan(begin_offset, line_offset + end, line_number, begin_column, static_cast<int>(end + 1)),
                 std::stoll(lexeme));
            index = end;
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
            std::size_t end = index + 1;
            while (end < line.size() &&
                   (std::isalnum(static_cast<unsigned char>(line[end])) != 0 || line[end] == '_')) {
                ++end;
            }
            const auto lexeme = std::string(line.substr(index, end - index));
            const auto keyword = kKeywords.find(lexeme);
            emit(keyword == kKeywords.end() ? TokenKind::Identifier : keyword->second,
                 lexeme,
                 makeSpan(begin_offset, line_offset + end, line_number, begin_column, static_cast<int>(end + 1)));
            index = end;
            continue;
        }

        if (ch == '"') {
            std::string value;
            std::size_t end = index + 1;
            while (end < line.size() && line[end] != '"') {
                if (line[end] == '\\') {
                    ++end;
                    if (end >= line.size()) {
                        fail(makeSpan(begin_offset, line_offset + end, line_number, begin_column, static_cast<int>(end + 1)),
                             "unterminated escape sequence in string literal");
                    }
                    switch (line[end]) {
                        case 'n':
                            value.push_back('\n');
                            break;
                        case 't':
                            value.push_back('\t');
                            break;
                        case '"':
                            value.push_back('"');
                            break;
                        case '\\':
                            value.push_back('\\');
                            break;
                        default:
                            fail(makeSpan(line_offset + end - 1,
                                          line_offset + end + 1,
                                          line_number,
                                          static_cast<int>(end),
                                          static_cast<int>(end + 2)),
                                 "unsupported escape sequence");
                    }
                    ++end;
                    continue;
                }
                value.push_back(line[end]);
                ++end;
            }
            if (end >= line.size() || line[end] != '"') {
                fail(makeSpan(begin_offset, line_offset + line.size(), line_number, begin_column, static_cast<int>(line.size() + 1)),
                     "unterminated string literal");
            }
            emit(TokenKind::String,
                 value,
                 makeSpan(begin_offset, line_offset + end + 1, line_number, begin_column, static_cast<int>(end + 2)));
            index = end + 1;
            continue;
        }

        const auto single = [&](const TokenKind kind) {
            emit(kind,
                 std::string(1, ch),
                 makeSpan(begin_offset, begin_offset + 1, line_number, begin_column, begin_column + 1));
        };

        switch (ch) {
            case '(':
                single(TokenKind::LeftParen);
                ++index;
                break;
            case ')':
                single(TokenKind::RightParen);
                ++index;
                break;
            case ',':
                single(TokenKind::Comma);
                ++index;
                break;
            case ':':
                single(TokenKind::Colon);
                ++index;
                break;
            case '+':
                single(TokenKind::Plus);
                ++index;
                break;
            case '-':
                if (index + 1 < line.size() && line[index + 1] == '>') {
                    emit(TokenKind::Arrow,
                         "->",
                         makeSpan(begin_offset, begin_offset + 2, line_number, begin_column, begin_column + 2));
                    index += 2;
                } else {
                    single(TokenKind::Minus);
                    ++index;
                }
                break;
            case '*':
                single(TokenKind::Star);
                ++index;
                break;
            case '/':
                single(TokenKind::Slash);
                ++index;
                break;
            case '%':
                single(TokenKind::Percent);
                ++index;
                break;
            case '=':
                if (index + 1 < line.size() && line[index + 1] == '=') {
                    emit(TokenKind::EqualEqual,
                         "==",
                         makeSpan(begin_offset, begin_offset + 2, line_number, begin_column, begin_column + 2));
                    index += 2;
                } else {
                    single(TokenKind::Equal);
                    ++index;
                }
                break;
            case '!':
                if (index + 1 < line.size() && line[index + 1] == '=') {
                    emit(TokenKind::BangEqual,
                         "!=",
                         makeSpan(begin_offset, begin_offset + 2, line_number, begin_column, begin_column + 2));
                    index += 2;
                } else {
                    fail(makeSpan(begin_offset, begin_offset + 1, line_number, begin_column, begin_column + 1),
                         "unexpected '!'; use 'not' for logical negation");
                }
                break;
            case '<':
                if (index + 1 < line.size() && line[index + 1] == '=') {
                    emit(TokenKind::LessEqual,
                         "<=",
                         makeSpan(begin_offset, begin_offset + 2, line_number, begin_column, begin_column + 2));
                    index += 2;
                } else {
                    single(TokenKind::Less);
                    ++index;
                }
                break;
            case '>':
                if (index + 1 < line.size() && line[index + 1] == '=') {
                    emit(TokenKind::GreaterEqual,
                         ">=",
                         makeSpan(begin_offset, begin_offset + 2, line_number, begin_column, begin_column + 2));
                    index += 2;
                } else {
                    single(TokenKind::Greater);
                    ++index;
                }
                break;
            default:
                fail(makeSpan(begin_offset, begin_offset + 1, line_number, begin_column, begin_column + 1),
                     "unexpected character '" + std::string(1, ch) + "'");
        }
    }
}

void Lexer::emitIndentation(const int width, const int line_number, const std::size_t offset) {
    const int current = indent_stack_.back();
    if (width > current) {
        indent_stack_.push_back(width);
        emit(TokenKind::Indent, "<indent>", makeSpan(offset, offset, line_number, 1, 1));
        return;
    }

    if (width == current) {
        return;
    }

    while (indent_stack_.size() > 1 && width < indent_stack_.back()) {
        indent_stack_.pop_back();
        emit(TokenKind::Dedent, "<dedent>", makeSpan(offset, offset, line_number, 1, 1));
    }

    if (indent_stack_.back() != width) {
        fail(makeSpan(offset, offset, line_number, 1, 1),
             "inconsistent indentation level; every dedent must match an earlier indentation width");
    }
}

void Lexer::emit(const TokenKind kind, std::string lexeme, const SourceSpan span, const std::int64_t integer_value) {
    tokens_.push_back(Token {
        .kind = kind,
        .lexeme = std::move(lexeme),
        .integer_value = integer_value,
        .span = span,
    });
}

SourceSpan Lexer::makeSpan(const std::size_t begin_offset,
                           const std::size_t end_offset,
                           const int line_number,
                           const int begin_column,
                           const int end_column) const {
    return SourceSpan {
        .begin = SourceLocation {.offset = begin_offset, .line = line_number, .column = begin_column},
        .end = SourceLocation {.offset = end_offset, .line = line_number, .column = end_column},
    };
}

[[noreturn]] void Lexer::fail(const SourceSpan& span, const std::string& message) {
    diagnostics_.error(span, message);
    throw CompilationError(diagnostics_.render());
}

}  // namespace zenasm

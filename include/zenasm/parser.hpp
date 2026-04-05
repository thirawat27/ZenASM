#pragma once

#include <memory>
#include <vector>

#include "zenasm/ast.hpp"
#include "zenasm/diagnostics.hpp"
#include "zenasm/token.hpp"

namespace zenasm {

class Parser {
  public:
    Parser(const std::vector<Token>& tokens, Diagnostics& diagnostics);

    [[nodiscard]] std::unique_ptr<Program> parse();

  private:
    [[nodiscard]] std::unique_ptr<Decl> parseDeclaration();
    [[nodiscard]] std::unique_ptr<Decl> parseFunction(bool is_extern);
    [[nodiscard]] std::vector<std::unique_ptr<Stmt>> parseBlock();
    [[nodiscard]] std::unique_ptr<Stmt> parseStatement();
    [[nodiscard]] std::unique_ptr<Stmt> parseIf();
    [[nodiscard]] std::unique_ptr<Stmt> parseWhile();
    [[nodiscard]] std::unique_ptr<Stmt> parseFor();
    [[nodiscard]] std::unique_ptr<Stmt> parseRepeat();
    [[nodiscard]] std::unique_ptr<Stmt> parseAsm();
    [[nodiscard]] std::unique_ptr<Stmt> parseElseBranchAsIf(const Token& start);
    [[nodiscard]] std::unique_ptr<Expr> parseExpression();
    [[nodiscard]] std::unique_ptr<Expr> parseOr();
    [[nodiscard]] std::unique_ptr<Expr> parseAnd();
    [[nodiscard]] std::unique_ptr<Expr> parseEquality();
    [[nodiscard]] std::unique_ptr<Expr> parseComparison();
    [[nodiscard]] std::unique_ptr<Expr> parseTerm();
    [[nodiscard]] std::unique_ptr<Expr> parseFactor();
    [[nodiscard]] std::unique_ptr<Expr> parseUnary();
    [[nodiscard]] std::unique_ptr<Expr> parseCall();
    [[nodiscard]] std::unique_ptr<Expr> parsePrimary();
    [[nodiscard]] std::vector<ParameterDecl> parseParameterList();
    [[nodiscard]] ValueType parseTypeName(bool allow_void, const std::string& context);
    [[nodiscard]] ValueType parseOptionalReturnType();

    [[nodiscard]] const Token& current() const noexcept;
    [[nodiscard]] const Token& previous() const noexcept;
    [[nodiscard]] bool isAtEnd() const noexcept;
    [[nodiscard]] bool check(TokenKind kind) const noexcept;
    [[nodiscard]] bool match(TokenKind kind) noexcept;
    [[nodiscard]] bool matchAny(std::initializer_list<TokenKind> kinds) noexcept;
    Token consume(TokenKind kind, const std::string& message);
    [[noreturn]] void fail(const SourceSpan& span, const std::string& message);
    [[nodiscard]] SourceSpan combine(const SourceSpan& start, const SourceSpan& end) const;

    const std::vector<Token>& tokens_;
    Diagnostics& diagnostics_;
    std::size_t current_index_ = 0;
};

}  // namespace zenasm

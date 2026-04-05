#include "zenasm/parser.hpp"

#include <typeinfo>

namespace zenasm {

Parser::Parser(const std::vector<Token>& tokens, Diagnostics& diagnostics)
    : tokens_(tokens),
      diagnostics_(diagnostics) {}

std::unique_ptr<Program> Parser::parse() {
    std::vector<std::unique_ptr<Decl>> declarations;
    while (!isAtEnd()) {
        while (match(TokenKind::Newline)) {
        }
        if (isAtEnd()) {
            break;
        }
        declarations.push_back(parseDeclaration());
    }
    return std::make_unique<Program>(std::move(declarations));
}

std::unique_ptr<Decl> Parser::parseDeclaration() {
    const bool is_extern = match(TokenKind::KeywordExtern);
    return parseFunction(is_extern);
}

std::unique_ptr<Decl> Parser::parseFunction(const bool is_extern) {
    const Token fn_token = consume(TokenKind::KeywordFn, "expected 'fn' to start a function declaration");
    const Token name = consume(TokenKind::Identifier, "expected function name");
    consume(TokenKind::LeftParen, "expected '(' after function name");
    const auto parameters = parseParameterList();
    consume(TokenKind::RightParen, "expected ')' after parameter list");
    const auto return_type = parseOptionalReturnType();

    if (is_extern) {
        consume(TokenKind::Newline, "expected newline after extern declaration");
        return std::make_unique<ExternFunctionDecl>(combine(fn_token.span, name.span), name.lexeme, parameters, return_type);
    }

    consume(TokenKind::Colon, "expected ':' after function signature");
    consume(TokenKind::Newline, "expected newline after function signature");
    auto body = parseBlock();
    return std::make_unique<FunctionDecl>(combine(fn_token.span, previous().span), name.lexeme, parameters, return_type, std::move(body));
}

std::vector<std::unique_ptr<Stmt>> Parser::parseBlock() {
    consume(TokenKind::Indent, "expected an indented block");
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TokenKind::Dedent) && !isAtEnd()) {
        statements.push_back(parseStatement());
    }
    consume(TokenKind::Dedent, "expected block to end with a dedent");
    return statements;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (match(TokenKind::KeywordLet)) {
        const Token start = previous();
        const Token name = consume(TokenKind::Identifier, "expected variable name after 'let'");
        consume(TokenKind::Equal, "expected '=' after variable name");
        auto initializer = parseExpression();
        consume(TokenKind::Newline, "expected newline after variable declaration");
        return std::make_unique<LetStmt>(combine(start.span, previous().span), name.lexeme, std::move(initializer));
    }

    if (match(TokenKind::KeywordReturn)) {
        const Token start = previous();
        std::unique_ptr<Expr> value;
        if (!check(TokenKind::Newline)) {
            value = parseExpression();
        }
        consume(TokenKind::Newline, "expected newline after return statement");
        return std::make_unique<ReturnStmt>(combine(start.span, previous().span), std::move(value));
    }

    if (match(TokenKind::KeywordIf)) {
        return parseIf();
    }

    if (match(TokenKind::KeywordWhile)) {
        return parseWhile();
    }

    if (match(TokenKind::KeywordFor)) {
        return parseFor();
    }

    if (match(TokenKind::KeywordRepeat)) {
        return parseRepeat();
    }

    if (match(TokenKind::KeywordAsm)) {
        return parseAsm();
    }

    if (match(TokenKind::KeywordBreak)) {
        const Token token = previous();
        consume(TokenKind::Newline, "expected newline after 'break'");
        return std::make_unique<BreakStmt>(token.span);
    }

    if (match(TokenKind::KeywordContinue)) {
        const Token token = previous();
        consume(TokenKind::Newline, "expected newline after 'continue'");
        return std::make_unique<ContinueStmt>(token.span);
    }

    if (check(TokenKind::Identifier) && current_index_ + 1 < tokens_.size() && tokens_[current_index_ + 1].kind == TokenKind::Equal) {
        const Token name = consume(TokenKind::Identifier, "expected assignment target");
        consume(TokenKind::Equal, "expected '=' in assignment");
        auto value = parseExpression();
        consume(TokenKind::Newline, "expected newline after assignment");
        return std::make_unique<AssignStmt>(combine(name.span, previous().span), name.lexeme, std::move(value));
    }

    auto expression = parseExpression();
    consume(TokenKind::Newline, "expected newline after expression");
    return std::make_unique<ExprStmt>(expression->span(), std::move(expression));
}

std::unique_ptr<Stmt> Parser::parseIf() {
    const Token start = previous();
    auto condition = parseExpression();
    consume(TokenKind::Colon, "expected ':' after if condition");
    consume(TokenKind::Newline, "expected newline after if condition");
    auto then_body = parseBlock();

    std::vector<std::unique_ptr<Stmt>> else_body;
    if (match(TokenKind::KeywordElif)) {
        else_body.push_back(parseElseBranchAsIf(previous()));
    } else if (match(TokenKind::KeywordElse)) {
        consume(TokenKind::Colon, "expected ':' after else");
        consume(TokenKind::Newline, "expected newline after else");
        else_body = parseBlock();
    }

    SourceSpan end_span = then_body.empty() ? condition->span() : then_body.back()->span();
    if (!else_body.empty()) {
        end_span = else_body.back()->span();
    }
    return std::make_unique<IfStmt>(combine(start.span, end_span), std::move(condition), std::move(then_body), std::move(else_body));
}

std::unique_ptr<Stmt> Parser::parseElseBranchAsIf(const Token& start) {
    auto condition = parseExpression();
    consume(TokenKind::Colon, "expected ':' after elif condition");
    consume(TokenKind::Newline, "expected newline after elif condition");
    auto then_body = parseBlock();

    std::vector<std::unique_ptr<Stmt>> else_body;
    if (match(TokenKind::KeywordElif)) {
        else_body.push_back(parseElseBranchAsIf(previous()));
    } else if (match(TokenKind::KeywordElse)) {
        consume(TokenKind::Colon, "expected ':' after else");
        consume(TokenKind::Newline, "expected newline after else");
        else_body = parseBlock();
    }

    SourceSpan end_span = then_body.empty() ? condition->span() : then_body.back()->span();
    if (!else_body.empty()) {
        end_span = else_body.back()->span();
    }
    return std::make_unique<IfStmt>(combine(start.span, end_span), std::move(condition), std::move(then_body), std::move(else_body));
}

std::unique_ptr<Stmt> Parser::parseWhile() {
    const Token start = previous();
    auto condition = parseExpression();
    consume(TokenKind::Colon, "expected ':' after while condition");
    consume(TokenKind::Newline, "expected newline after while condition");
    auto body = parseBlock();
    const SourceSpan end_span = body.empty() ? condition->span() : body.back()->span();
    return std::make_unique<WhileStmt>(combine(start.span, end_span), std::move(condition), std::move(body));
}

std::unique_ptr<Stmt> Parser::parseFor() {
    const Token start = previous();
    const Token variable = consume(TokenKind::Identifier, "expected loop variable after 'for'");
    consume(TokenKind::KeywordIn, "expected 'in' after loop variable");
    const Token range_name = consume(TokenKind::Identifier, "expected 'range' after 'in'");
    if (range_name.lexeme != "range") {
        fail(range_name.span, "expected 'range(...)' in for loop");
    }
    consume(TokenKind::LeftParen, "expected '(' after range");

    std::vector<std::unique_ptr<Expr>> arguments;
    if (!check(TokenKind::RightParen)) {
        do {
            arguments.push_back(parseExpression());
        } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RightParen, "expected ')' after range arguments");
    consume(TokenKind::Colon, "expected ':' after for range");
    consume(TokenKind::Newline, "expected newline after for loop header");
    auto body = parseBlock();

    if (arguments.empty() || arguments.size() > 3) {
        fail(range_name.span, "range expects 1 to 3 arguments");
    }

    std::unique_ptr<Expr> loop_start;
    std::unique_ptr<Expr> loop_end;
    std::unique_ptr<Expr> loop_step;
    if (arguments.size() == 1) {
        loop_start = std::make_unique<IntegerExpr>(arguments.front()->span(), 0);
        loop_end = std::move(arguments[0]);
        loop_step = std::make_unique<IntegerExpr>(loop_end->span(), 1);
    } else if (arguments.size() == 2) {
        loop_start = std::move(arguments[0]);
        loop_end = std::move(arguments[1]);
        loop_step = std::make_unique<IntegerExpr>(loop_end->span(), 1);
    } else {
        loop_start = std::move(arguments[0]);
        loop_end = std::move(arguments[1]);
        loop_step = std::move(arguments[2]);
    }

    const SourceSpan end_span = body.empty() ? loop_end->span() : body.back()->span();
    return std::make_unique<ForStmt>(
        combine(start.span, end_span), variable.lexeme, std::move(loop_start), std::move(loop_end), std::move(loop_step), std::move(body));
}

std::unique_ptr<Stmt> Parser::parseRepeat() {
    const Token start = previous();
    auto count = parseExpression();
    consume(TokenKind::Colon, "expected ':' after repeat count");
    consume(TokenKind::Newline, "expected newline after repeat count");
    auto body = parseBlock();
    const SourceSpan end_span = body.empty() ? count->span() : body.back()->span();
    return std::make_unique<RepeatStmt>(combine(start.span, end_span), std::move(count), std::move(body));
}

std::unique_ptr<Stmt> Parser::parseAsm() {
    const Token start = previous();
    std::vector<std::unique_ptr<Expr>> operands;

    if (match(TokenKind::LeftParen)) {
        if (!check(TokenKind::RightParen)) {
            do {
                operands.push_back(parseExpression());
            } while (match(TokenKind::Comma));
        }
        consume(TokenKind::RightParen, "expected ')' after asm operand list");
    }
    std::vector<std::string> lines;

    if (operands.empty() && match(TokenKind::String)) {
        lines.push_back(previous().lexeme);
        consume(TokenKind::Newline, "expected newline after inline asm string");
        return std::make_unique<InlineAsmStmt>(combine(start.span, previous().span), std::move(lines), std::move(operands));
    }

    consume(TokenKind::Colon, "expected ':' or a string literal after 'asm'");
    consume(TokenKind::Newline, "expected newline after asm block header");
    consume(TokenKind::Indent, "expected indented asm block");
    while (!check(TokenKind::Dedent) && !isAtEnd()) {
        const Token line = consume(TokenKind::String, "expected a string literal inside asm block");
        lines.push_back(line.lexeme);
        consume(TokenKind::Newline, "expected newline after asm line");
    }
    const Token end = consume(TokenKind::Dedent, "expected end of asm block");
    return std::make_unique<InlineAsmStmt>(combine(start.span, end.span), std::move(lines), std::move(operands));
}

std::unique_ptr<Expr> Parser::parseExpression() {
    return parseOr();
}

std::unique_ptr<Expr> Parser::parseOr() {
    auto expr = parseAnd();
    while (match(TokenKind::KeywordOr)) {
        auto rhs = parseAnd();
        expr = std::make_unique<BinaryExpr>(combine(expr->span(), rhs->span()), BinaryOp::LogicalOr, std::move(expr), std::move(rhs));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseAnd() {
    auto expr = parseEquality();
    while (match(TokenKind::KeywordAnd)) {
        auto rhs = parseEquality();
        expr = std::make_unique<BinaryExpr>(combine(expr->span(), rhs->span()), BinaryOp::LogicalAnd, std::move(expr), std::move(rhs));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto expr = parseComparison();
    while (matchAny({TokenKind::EqualEqual, TokenKind::BangEqual})) {
        const Token op = previous();
        auto rhs = parseComparison();
        const auto kind = op.kind == TokenKind::EqualEqual ? BinaryOp::Equal : BinaryOp::NotEqual;
        expr = std::make_unique<BinaryExpr>(combine(expr->span(), rhs->span()), kind, std::move(expr), std::move(rhs));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseComparison() {
    auto expr = parseTerm();
    while (matchAny({TokenKind::Less, TokenKind::LessEqual, TokenKind::Greater, TokenKind::GreaterEqual})) {
        const Token op = previous();
        auto rhs = parseTerm();
        BinaryOp kind = BinaryOp::Less;
        switch (op.kind) {
            case TokenKind::Less:
                kind = BinaryOp::Less;
                break;
            case TokenKind::LessEqual:
                kind = BinaryOp::LessEqual;
                break;
            case TokenKind::Greater:
                kind = BinaryOp::Greater;
                break;
            case TokenKind::GreaterEqual:
                kind = BinaryOp::GreaterEqual;
                break;
            default:
                break;
        }
        expr = std::make_unique<BinaryExpr>(combine(expr->span(), rhs->span()), kind, std::move(expr), std::move(rhs));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseTerm() {
    auto expr = parseFactor();
    while (matchAny({TokenKind::Plus, TokenKind::Minus})) {
        const Token op = previous();
        auto rhs = parseFactor();
        const auto kind = op.kind == TokenKind::Plus ? BinaryOp::Add : BinaryOp::Sub;
        expr = std::make_unique<BinaryExpr>(combine(expr->span(), rhs->span()), kind, std::move(expr), std::move(rhs));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseFactor() {
    auto expr = parseUnary();
    while (matchAny({TokenKind::Star, TokenKind::Slash, TokenKind::Percent})) {
        const Token op = previous();
        auto rhs = parseUnary();
        BinaryOp kind = BinaryOp::Mul;
        switch (op.kind) {
            case TokenKind::Star:
                kind = BinaryOp::Mul;
                break;
            case TokenKind::Slash:
                kind = BinaryOp::Div;
                break;
            case TokenKind::Percent:
                kind = BinaryOp::Mod;
                break;
            default:
                break;
        }
        expr = std::make_unique<BinaryExpr>(combine(expr->span(), rhs->span()), kind, std::move(expr), std::move(rhs));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (match(TokenKind::Minus)) {
        const Token op = previous();
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(combine(op.span, operand->span()), UnaryOp::Negate, std::move(operand));
    }
    if (match(TokenKind::KeywordNot)) {
        const Token op = previous();
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(combine(op.span, operand->span()), UnaryOp::LogicalNot, std::move(operand));
    }
    return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall() {
    auto expr = parsePrimary();

    while (match(TokenKind::LeftParen)) {
        auto* identifier = dynamic_cast<IdentifierExpr*>(expr.get());
        if (identifier == nullptr) {
            fail(previous().span, "only named functions can be called");
        }

        std::vector<std::unique_ptr<Expr>> arguments;
        if (!check(TokenKind::RightParen)) {
            do {
                arguments.push_back(parseExpression());
            } while (match(TokenKind::Comma));
        }
        const Token right_paren = consume(TokenKind::RightParen, "expected ')' after call arguments");
        expr = std::make_unique<CallExpr>(combine(expr->span(), right_paren.span), identifier->name, std::move(arguments));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenKind::Integer)) {
        const Token token = previous();
        return std::make_unique<IntegerExpr>(token.span, token.integer_value);
    }
    if (match(TokenKind::KeywordTrue)) {
        return std::make_unique<BoolExpr>(previous().span, true);
    }
    if (match(TokenKind::KeywordFalse)) {
        return std::make_unique<BoolExpr>(previous().span, false);
    }
    if (match(TokenKind::String)) {
        const Token token = previous();
        return std::make_unique<StringExpr>(token.span, token.lexeme);
    }
    if (match(TokenKind::Identifier)) {
        const Token token = previous();
        return std::make_unique<IdentifierExpr>(token.span, token.lexeme);
    }
    if (match(TokenKind::LeftParen)) {
        auto expr = parseExpression();
        consume(TokenKind::RightParen, "expected ')' after expression");
        return expr;
    }

    fail(current().span, "expected an expression");
}

std::vector<ParameterDecl> Parser::parseParameterList() {
    std::vector<ParameterDecl> parameters;
    if (check(TokenKind::RightParen)) {
        return parameters;
    }

    do {
        const Token name = consume(TokenKind::Identifier, "expected parameter name");
        ValueType type = ValueType::Unknown;
        if (match(TokenKind::Colon)) {
            type = parseTypeName(false, "parameter type");
        }
        parameters.push_back(ParameterDecl {.name = name.lexeme, .type = type, .span = name.span});
    } while (match(TokenKind::Comma));

    return parameters;
}

ValueType Parser::parseTypeName(const bool allow_void, const std::string& context) {
    const Token type_token = consume(TokenKind::Identifier, "expected " + context);
    if (type_token.lexeme == "i64") {
        return ValueType::Int64;
    }
    if (type_token.lexeme == "bool") {
        return ValueType::Bool;
    }
    if (type_token.lexeme == "str") {
        return ValueType::String;
    }
    if (allow_void && type_token.lexeme == "void") {
        return ValueType::Void;
    }
    fail(type_token.span, "unknown type '" + type_token.lexeme + "'");
}

ValueType Parser::parseOptionalReturnType() {
    if (!match(TokenKind::Arrow)) {
        return ValueType::Unknown;
    }
    return parseTypeName(true, "return type");
}

const Token& Parser::current() const noexcept {
    return tokens_[current_index_];
}

const Token& Parser::previous() const noexcept {
    return tokens_[current_index_ - 1];
}

bool Parser::isAtEnd() const noexcept {
    return current().kind == TokenKind::EndOfFile;
}

bool Parser::check(const TokenKind kind) const noexcept {
    return current().kind == kind;
}

bool Parser::match(const TokenKind kind) noexcept {
    if (!check(kind)) {
        return false;
    }
    ++current_index_;
    return true;
}

bool Parser::matchAny(const std::initializer_list<TokenKind> kinds) noexcept {
    for (const auto kind : kinds) {
        if (check(kind)) {
            ++current_index_;
            return true;
        }
    }
    return false;
}

Token Parser::consume(const TokenKind kind, const std::string& message) {
    if (!check(kind)) {
        fail(current().span, message + " (found '" + std::string(toString(current().kind)) + "')");
    }
    return tokens_[current_index_++];
}

[[noreturn]] void Parser::fail(const SourceSpan& span, const std::string& message) {
    diagnostics_.error(span, message);
    throw CompilationError(diagnostics_.render());
}

SourceSpan Parser::combine(const SourceSpan& start, const SourceSpan& end) const {
    return SourceSpan {.begin = start.begin, .end = end.end};
}

}  // namespace zenasm

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "zenasm/source.hpp"

namespace zenasm {

enum class ValueType {
    Unknown,
    Void,
    Int64,
    Bool,
    String,
};

struct ParameterDecl {
    std::string name;
    ValueType type = ValueType::Unknown;
    SourceSpan span {};
};

class AstNode {
  public:
    explicit AstNode(SourceSpan span)
        : span_(span) {}
    virtual ~AstNode() = default;

    [[nodiscard]] const SourceSpan& span() const noexcept { return span_; }

  private:
    SourceSpan span_;
};

class Expr : public AstNode {
  public:
    using AstNode::AstNode;
    ~Expr() override = default;
};

class Stmt : public AstNode {
  public:
    using AstNode::AstNode;
    ~Stmt() override = default;
};

class Decl : public AstNode {
  public:
    using AstNode::AstNode;
    ~Decl() override = default;
};

enum class UnaryOp {
    Negate,
    LogicalNot,
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LogicalAnd,
    LogicalOr,
};

class IntegerExpr final : public Expr {
  public:
    IntegerExpr(SourceSpan span, std::int64_t value)
        : Expr(span), value(value) {}

    std::int64_t value;
};

class BoolExpr final : public Expr {
  public:
    BoolExpr(SourceSpan span, bool value)
        : Expr(span), value(value) {}

    bool value;
};

class StringExpr final : public Expr {
  public:
    StringExpr(SourceSpan span, std::string value)
        : Expr(span), value(std::move(value)) {}

    std::string value;
};

class IdentifierExpr final : public Expr {
  public:
    IdentifierExpr(SourceSpan span, std::string name)
        : Expr(span), name(std::move(name)) {}

    std::string name;
};

class UnaryExpr final : public Expr {
  public:
    UnaryExpr(SourceSpan span, UnaryOp op, std::unique_ptr<Expr> operand)
        : Expr(span), op(op), operand(std::move(operand)) {}

    UnaryOp op;
    std::unique_ptr<Expr> operand;
};

class BinaryExpr final : public Expr {
  public:
    BinaryExpr(SourceSpan span, BinaryOp op, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
        : Expr(span), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    BinaryOp op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
};

class CallExpr final : public Expr {
  public:
    CallExpr(SourceSpan span, std::string callee, std::vector<std::unique_ptr<Expr>> arguments)
        : Expr(span), callee(std::move(callee)), arguments(std::move(arguments)) {}

    std::string callee;
    std::vector<std::unique_ptr<Expr>> arguments;
};

class LetStmt final : public Stmt {
  public:
    LetStmt(SourceSpan span, std::string name, std::unique_ptr<Expr> initializer)
        : Stmt(span), name(std::move(name)), initializer(std::move(initializer)) {}

    std::string name;
    std::unique_ptr<Expr> initializer;
};

class AssignStmt final : public Stmt {
  public:
    AssignStmt(SourceSpan span, std::string name, std::unique_ptr<Expr> value)
        : Stmt(span), name(std::move(name)), value(std::move(value)) {}

    std::string name;
    std::unique_ptr<Expr> value;
};

class ReturnStmt final : public Stmt {
  public:
    ReturnStmt(SourceSpan span, std::unique_ptr<Expr> value)
        : Stmt(span), value(std::move(value)) {}

    std::unique_ptr<Expr> value;
};

class BreakStmt final : public Stmt {
  public:
    explicit BreakStmt(SourceSpan span)
        : Stmt(span) {}
};

class ContinueStmt final : public Stmt {
  public:
    explicit ContinueStmt(SourceSpan span)
        : Stmt(span) {}
};

class ExprStmt final : public Stmt {
  public:
    ExprStmt(SourceSpan span, std::unique_ptr<Expr> expr)
        : Stmt(span), expr(std::move(expr)) {}

    std::unique_ptr<Expr> expr;
};

class InlineAsmStmt final : public Stmt {
  public:
    InlineAsmStmt(SourceSpan span, std::vector<std::string> lines, std::vector<std::unique_ptr<Expr>> operands)
        : Stmt(span), lines(std::move(lines)), operands(std::move(operands)) {}

    std::vector<std::string> lines;
    std::vector<std::unique_ptr<Expr>> operands;
};

class IfStmt final : public Stmt {
  public:
    IfStmt(SourceSpan span,
           std::unique_ptr<Expr> condition,
           std::vector<std::unique_ptr<Stmt>> then_body,
           std::vector<std::unique_ptr<Stmt>> else_body)
        : Stmt(span),
          condition(std::move(condition)),
          then_body(std::move(then_body)),
          else_body(std::move(else_body)) {}

    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> then_body;
    std::vector<std::unique_ptr<Stmt>> else_body;
};

class WhileStmt final : public Stmt {
  public:
    WhileStmt(SourceSpan span, std::unique_ptr<Expr> condition, std::vector<std::unique_ptr<Stmt>> body)
        : Stmt(span), condition(std::move(condition)), body(std::move(body)) {}

    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> body;
};

class RepeatStmt final : public Stmt {
  public:
    RepeatStmt(SourceSpan span, std::unique_ptr<Expr> count, std::vector<std::unique_ptr<Stmt>> body)
        : Stmt(span), count(std::move(count)), body(std::move(body)) {}

    std::unique_ptr<Expr> count;
    std::vector<std::unique_ptr<Stmt>> body;
};

class ForStmt final : public Stmt {
  public:
    ForStmt(SourceSpan span,
            std::string variable_name,
            std::unique_ptr<Expr> start,
            std::unique_ptr<Expr> end,
            std::unique_ptr<Expr> step,
            std::vector<std::unique_ptr<Stmt>> body)
        : Stmt(span),
          variable_name(std::move(variable_name)),
          start(std::move(start)),
          end(std::move(end)),
          step(std::move(step)),
          body(std::move(body)) {}

    std::string variable_name;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    std::unique_ptr<Expr> step;
    std::vector<std::unique_ptr<Stmt>> body;
};

class FunctionDecl final : public Decl {
  public:
    FunctionDecl(SourceSpan span,
                 std::string name,
                 std::vector<ParameterDecl> parameters,
                 ValueType return_type,
                 std::vector<std::unique_ptr<Stmt>> body)
        : Decl(span),
          name(std::move(name)),
          parameters(std::move(parameters)),
          return_type(return_type),
          body(std::move(body)) {}

    std::string name;
    std::vector<ParameterDecl> parameters;
    ValueType return_type = ValueType::Unknown;
    std::vector<std::unique_ptr<Stmt>> body;
};

class ExternFunctionDecl final : public Decl {
  public:
    ExternFunctionDecl(SourceSpan span, std::string name, std::vector<ParameterDecl> parameters, ValueType return_type)
        : Decl(span), name(std::move(name)), parameters(std::move(parameters)), return_type(return_type) {}

    std::string name;
    std::vector<ParameterDecl> parameters;
    ValueType return_type = ValueType::Unknown;
};

class Program final : public AstNode {
  public:
    explicit Program(std::vector<std::unique_ptr<Decl>> declarations)
        : AstNode(SourceSpan {}), declarations(std::move(declarations)) {}

    std::vector<std::unique_ptr<Decl>> declarations;
};

[[nodiscard]] std::string toString(ValueType type);
[[nodiscard]] std::string toString(UnaryOp op);
[[nodiscard]] std::string toString(BinaryOp op);

}  // namespace zenasm

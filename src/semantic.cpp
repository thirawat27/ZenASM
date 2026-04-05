#include "zenasm/semantic.hpp"

#include <memory>

namespace zenasm {

SemanticAnalyzer::SemanticAnalyzer(Diagnostics& diagnostics, SemanticModel& model)
    : diagnostics_(diagnostics),
      model_(model) {}

void SemanticAnalyzer::analyze(const Program& program) {
    declareFunctions(program);
    for (const auto& decl : program.declarations) {
        if (const auto* function = dynamic_cast<const FunctionDecl*>(decl.get())) {
            analyzeFunction(*function);
        }
    }
}

void SemanticAnalyzer::declareFunctions(const Program& program) {
    model_.functions.clear();

    for (const auto& decl : program.declarations) {
        if (const auto* function = dynamic_cast<const FunctionDecl*>(decl.get())) {
            if (model_.functions.contains(function->name)) {
                fail(function->span(), "duplicate function declaration for '" + function->name + "'");
            }
            std::vector<ValueType> parameter_types;
            parameter_types.reserve(function->parameters.size());
            for (const auto& parameter : function->parameters) {
                parameter_types.push_back(parameter.type == ValueType::Unknown ? ValueType::Int64 : parameter.type);
            }
            model_.functions.emplace(function->name, FunctionSignature {
                                                         .name = function->name,
                                                         .is_extern = false,
                                                         .parameter_types = std::move(parameter_types),
                                                         .return_type = function->return_type,
                                                         .explicit_return_type = function->return_type != ValueType::Unknown,
                                                     });
            continue;
        }

        const auto* external = dynamic_cast<const ExternFunctionDecl*>(decl.get());
        if (external == nullptr) {
            continue;
        }
        if (model_.functions.contains(external->name)) {
            fail(external->span(), "duplicate function declaration for '" + external->name + "'");
        }
        std::vector<ValueType> parameter_types;
        parameter_types.reserve(external->parameters.size());
        for (const auto& parameter : external->parameters) {
            parameter_types.push_back(parameter.type == ValueType::Unknown ? ValueType::Int64 : parameter.type);
        }
        model_.functions.emplace(external->name, FunctionSignature {
                                                     .name = external->name,
                                                     .is_extern = true,
                                                     .parameter_types = std::move(parameter_types),
                                                     .return_type = external->return_type == ValueType::Unknown ? ValueType::Int64 : external->return_type,
                                                     .explicit_return_type = external->return_type != ValueType::Unknown,
                                                 });
    }
}

void SemanticAnalyzer::analyzeFunction(const FunctionDecl& function) {
    current_function_ = lookupFunction(function.name);
    current_function_has_return_ = false;
    pushScope();
    for (std::size_t index = 0; index < function.parameters.size(); ++index) {
        declareVariable(function.parameters[index].name, current_function_->parameter_types[index], function.parameters[index].span);
    }
    analyzeBlock(function.body);
    popScope();
    if (current_function_->return_type == ValueType::Unknown) {
        current_function_->return_type = ValueType::Void;
    }
    if (current_function_->explicit_return_type && current_function_->return_type != ValueType::Void && !current_function_has_return_) {
        fail(function.span(), "function '" + function.name + "' declares return type '" + toString(current_function_->return_type) +
                                  "' but has no return statement");
    }
    current_function_ = nullptr;
}

void SemanticAnalyzer::analyzeStatement(const Stmt& statement) {
    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const auto type = analyzeExpression(*let->initializer);
        declareVariable(let->name, type, let->span());
        return;
    }

    if (const auto* assign = dynamic_cast<const AssignStmt*>(&statement)) {
        const auto target_type = lookupVariable(assign->name, assign->span());
        const auto value_type = analyzeExpression(*assign->value);
        if (!canAssign(target_type, value_type)) {
            fail(assign->span(),
                 "cannot assign value of type '" + toString(value_type) + "' to variable '" + assign->name + "' of type '" +
                     toString(target_type) + "'");
        }
        return;
    }

    if (const auto* ret = dynamic_cast<const ReturnStmt*>(&statement)) {
        const auto value_type = ret->value == nullptr ? ValueType::Void : analyzeExpression(*ret->value);
        current_function_->return_type = mergeReturnType(current_function_->return_type, value_type, ret->span());
        current_function_has_return_ = true;
        return;
    }

    if (const auto* expr = dynamic_cast<const ExprStmt*>(&statement)) {
        static_cast<void>(analyzeExpression(*expr->expr));
        return;
    }

    if (dynamic_cast<const BreakStmt*>(&statement) != nullptr) {
        if (loop_depth_ <= 0) {
            fail(statement.span(), "'break' can only be used inside a loop");
        }
        return;
    }

    if (dynamic_cast<const ContinueStmt*>(&statement) != nullptr) {
        if (loop_depth_ <= 0) {
            fail(statement.span(), "'continue' can only be used inside a loop");
        }
        return;
    }

    if (dynamic_cast<const InlineAsmStmt*>(&statement) != nullptr) {
        const auto* asm_stmt = static_cast<const InlineAsmStmt*>(&statement);
        for (const auto& operand : asm_stmt->operands) {
            const auto type = analyzeExpression(*operand);
            if (type != ValueType::Int64 && type != ValueType::Bool && type != ValueType::String) {
                fail(operand->span(), "inline asm operands must be i64, bool, or str");
            }
        }
        return;
    }

    if (const auto* if_stmt = dynamic_cast<const IfStmt*>(&statement)) {
        const auto condition_type = analyzeExpression(*if_stmt->condition);
        if (condition_type != ValueType::Bool) {
            fail(if_stmt->condition->span(), "if condition must be of type 'bool'");
        }
        pushScope();
        analyzeBlock(if_stmt->then_body);
        popScope();
        pushScope();
        analyzeBlock(if_stmt->else_body);
        popScope();
        return;
    }

    if (const auto* while_stmt = dynamic_cast<const WhileStmt*>(&statement)) {
        const auto condition_type = analyzeExpression(*while_stmt->condition);
        if (condition_type != ValueType::Bool) {
            fail(while_stmt->condition->span(), "while condition must be of type 'bool'");
        }
        ++loop_depth_;
        pushScope();
        analyzeBlock(while_stmt->body);
        popScope();
        --loop_depth_;
        return;
    }

    if (const auto* for_stmt = dynamic_cast<const ForStmt*>(&statement)) {
        const auto start_type = analyzeExpression(*for_stmt->start);
        const auto end_type = analyzeExpression(*for_stmt->end);
        const auto step_type = analyzeExpression(*for_stmt->step);
        if (start_type != ValueType::Int64 || end_type != ValueType::Int64 || step_type != ValueType::Int64) {
            fail(for_stmt->span(), "for-loop range expressions must be i64");
        }
        ++loop_depth_;
        pushScope();
        declareVariable(for_stmt->variable_name, ValueType::Int64, for_stmt->span());
        analyzeBlock(for_stmt->body);
        popScope();
        --loop_depth_;
        return;
    }

    if (const auto* repeat_stmt = dynamic_cast<const RepeatStmt*>(&statement)) {
        const auto count_type = analyzeExpression(*repeat_stmt->count);
        if (count_type != ValueType::Int64) {
            fail(repeat_stmt->count->span(), "repeat count must be of type 'i64'");
        }
        ++loop_depth_;
        pushScope();
        analyzeBlock(repeat_stmt->body);
        popScope();
        --loop_depth_;
        return;
    }

    fail(statement.span(), "unsupported statement in semantic analyzer");
}

void SemanticAnalyzer::analyzeBlock(const std::vector<std::unique_ptr<Stmt>>& statements) {
    for (const auto& statement : statements) {
        analyzeStatement(*statement);
    }
}

ValueType SemanticAnalyzer::analyzeExpression(const Expr& expr) {
    if (dynamic_cast<const IntegerExpr*>(&expr) != nullptr) {
        model_.expr_types[&expr] = ValueType::Int64;
        return ValueType::Int64;
    }

    if (dynamic_cast<const BoolExpr*>(&expr) != nullptr) {
        model_.expr_types[&expr] = ValueType::Bool;
        return ValueType::Bool;
    }

    if (dynamic_cast<const StringExpr*>(&expr) != nullptr) {
        model_.expr_types[&expr] = ValueType::String;
        return ValueType::String;
    }

    if (const auto* identifier = dynamic_cast<const IdentifierExpr*>(&expr)) {
        const auto type = lookupVariable(identifier->name, identifier->span());
        model_.expr_types[&expr] = type;
        return type;
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
        const auto operand_type = analyzeExpression(*unary->operand);
        ValueType result = ValueType::Unknown;
        switch (unary->op) {
            case UnaryOp::Negate:
                if (operand_type != ValueType::Int64) {
                    fail(unary->span(), "unary '-' expects an i64 operand");
                }
                result = ValueType::Int64;
                break;
            case UnaryOp::LogicalNot:
                if (operand_type != ValueType::Bool) {
                    fail(unary->span(), "'not' expects a bool operand");
                }
                result = ValueType::Bool;
                break;
        }
        model_.expr_types[&expr] = result;
        return result;
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
        const auto lhs_type = analyzeExpression(*binary->lhs);
        const auto rhs_type = analyzeExpression(*binary->rhs);
        ValueType result = ValueType::Unknown;
        switch (binary->op) {
            case BinaryOp::Add:
            case BinaryOp::Sub:
            case BinaryOp::Mul:
            case BinaryOp::Div:
            case BinaryOp::Mod:
                if (lhs_type != ValueType::Int64 || rhs_type != ValueType::Int64) {
                    fail(binary->span(), "arithmetic operators require i64 operands");
                }
                result = ValueType::Int64;
                break;
            case BinaryOp::Less:
            case BinaryOp::LessEqual:
            case BinaryOp::Greater:
            case BinaryOp::GreaterEqual:
                if (lhs_type != ValueType::Int64 || rhs_type != ValueType::Int64) {
                    fail(binary->span(), "comparison operators require i64 operands");
                }
                result = ValueType::Bool;
                break;
            case BinaryOp::Equal:
            case BinaryOp::NotEqual:
                if (!canAssign(lhs_type, rhs_type) && !canAssign(rhs_type, lhs_type)) {
                    fail(binary->span(), "equality operators require comparable operand types");
                }
                result = ValueType::Bool;
                break;
            case BinaryOp::LogicalAnd:
            case BinaryOp::LogicalOr:
                if (lhs_type != ValueType::Bool || rhs_type != ValueType::Bool) {
                    fail(binary->span(), "logical operators require bool operands");
                }
                result = ValueType::Bool;
                break;
        }
        model_.expr_types[&expr] = result;
        return result;
    }

    if (const auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        if (call->callee == "print") {
            if (call->arguments.size() != 1) {
                fail(call->span(), "print expects exactly 1 argument");
            }
            const auto argument_type = analyzeExpression(*call->arguments[0]);
            if (argument_type != ValueType::Int64 && argument_type != ValueType::Bool && argument_type != ValueType::String) {
                fail(call->arguments[0]->span(), "print supports i64, bool, and str");
            }
            model_.expr_types[&expr] = ValueType::Void;
            return ValueType::Void;
        }

        auto* function = lookupFunction(call->callee);
        if (function == nullptr) {
            fail(call->span(), "unknown function '" + call->callee + "'");
        }
        if (!function->explicit_return_type && function->return_type == ValueType::Unknown) {
            fail(call->span(),
                 "function '" + call->callee + "' needs an explicit return type before it can be used here");
        }
        if (call->arguments.size() != function->parameter_types.size()) {
            fail(call->span(),
                 "function '" + call->callee + "' expects " + std::to_string(function->parameter_types.size()) + " argument(s) but got " +
                     std::to_string(call->arguments.size()));
        }

        for (std::size_t index = 0; index < call->arguments.size(); ++index) {
            const auto argument_type = analyzeExpression(*call->arguments[index]);
            if (!canAssign(function->parameter_types[index], argument_type)) {
                fail(call->arguments[index]->span(),
                     "argument " + std::to_string(index + 1) + " to '" + call->callee + "' must be '" +
                         toString(function->parameter_types[index]) + "', got '" + toString(argument_type) + "'");
            }
        }

        model_.expr_types[&expr] = function->return_type;
        return function->return_type;
    }

    fail(expr.span(), "unsupported expression in semantic analyzer");
}

void SemanticAnalyzer::pushScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::popScope() {
    scopes_.pop_back();
}

void SemanticAnalyzer::declareVariable(const std::string& name, const ValueType type, const SourceSpan& span) {
    if (scopes_.empty()) {
        pushScope();
    }
    auto& scope = scopes_.back();
    if (scope.contains(name)) {
        fail(span, "duplicate variable declaration for '" + name + "' in the same scope");
    }
    scope.emplace(name, type);
}

ValueType SemanticAnalyzer::lookupVariable(const std::string& name, const SourceSpan& span) const {
    for (auto iter = scopes_.rbegin(); iter != scopes_.rend(); ++iter) {
        const auto found = iter->find(name);
        if (found != iter->end()) {
            return found->second;
        }
    }
    fail(span, "unknown variable '" + name + "'");
}

FunctionSignature* SemanticAnalyzer::lookupFunction(const std::string& name) {
    const auto it = model_.functions.find(name);
    if (it == model_.functions.end()) {
        return nullptr;
    }
    return &it->second;
}

const FunctionSignature* SemanticAnalyzer::lookupFunction(const std::string& name) const {
    const auto it = model_.functions.find(name);
    if (it == model_.functions.end()) {
        return nullptr;
    }
    return &it->second;
}

bool SemanticAnalyzer::canAssign(const ValueType target, const ValueType source) const noexcept {
    if (target == ValueType::Unknown || source == ValueType::Unknown) {
        return true;
    }
    if (target == source) {
        return true;
    }
    return target == ValueType::Int64 && source == ValueType::Bool;
}

ValueType SemanticAnalyzer::mergeReturnType(const ValueType current, const ValueType candidate, const SourceSpan& span) {
    if (current_function_ != nullptr && current_function_->explicit_return_type) {
        if (!canAssign(current_function_->return_type, candidate)) {
            fail(span,
                 "function return type is declared as '" + toString(current_function_->return_type) + "' but got '" + toString(candidate) +
                     "'");
        }
        return current_function_->return_type;
    }
    if (current == ValueType::Unknown) {
        return candidate;
    }
    if (current == candidate) {
        return current;
    }
    fail(span, "function return type changed from '" + toString(current) + "' to '" + toString(candidate) + "'");
}

[[noreturn]] void SemanticAnalyzer::fail(const SourceSpan& span, const std::string& message) const {
    diagnostics_.error(span, message);
    throw CompilationError(diagnostics_.render());
}

}  // namespace zenasm

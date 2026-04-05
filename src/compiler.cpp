#include "zenasm/compiler.hpp"

#include <sstream>

#include "zenasm/diagnostics.hpp"
#include "zenasm/lexer.hpp"
#include "zenasm/lowering.hpp"
#include "zenasm/optimizer.hpp"
#include "zenasm/parser.hpp"
#include "zenasm/semantic.hpp"
#include "zenasm/source.hpp"

namespace zenasm {

namespace {

void dumpExpression(const Expr& expr, std::ostringstream& out, const int indent);
void dumpStatement(const Stmt& stmt, std::ostringstream& out, const int indent);

std::string indent(const int depth) {
    return std::string(static_cast<std::size_t>(depth * 2), ' ');
}

void dumpExpression(const Expr& expr, std::ostringstream& out, const int depth) {
    if (const auto* integer = dynamic_cast<const IntegerExpr*>(&expr)) {
        out << indent(depth) << "Integer(" << integer->value << ")\n";
        return;
    }
    if (const auto* boolean = dynamic_cast<const BoolExpr*>(&expr)) {
        out << indent(depth) << "Bool(" << (boolean->value ? "true" : "false") << ")\n";
        return;
    }
    if (const auto* string = dynamic_cast<const StringExpr*>(&expr)) {
        out << indent(depth) << "String(\"" << string->value << "\")\n";
        return;
    }
    if (const auto* identifier = dynamic_cast<const IdentifierExpr*>(&expr)) {
        out << indent(depth) << "Identifier(" << identifier->name << ")\n";
        return;
    }
    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
        out << indent(depth) << "Unary(" << toString(unary->op) << ")\n";
        dumpExpression(*unary->operand, out, depth + 1);
        return;
    }
    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
        out << indent(depth) << "Binary(" << toString(binary->op) << ")\n";
        dumpExpression(*binary->lhs, out, depth + 1);
        dumpExpression(*binary->rhs, out, depth + 1);
        return;
    }
    if (const auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        out << indent(depth) << "Call(" << call->callee << ")\n";
        for (const auto& argument : call->arguments) {
            dumpExpression(*argument, out, depth + 1);
        }
        return;
    }
    out << indent(depth) << "<unknown expr>\n";
}

void dumpStatement(const Stmt& stmt, std::ostringstream& out, const int depth) {
    if (const auto* let = dynamic_cast<const LetStmt*>(&stmt)) {
        out << indent(depth) << "Let(" << let->name << ")\n";
        dumpExpression(*let->initializer, out, depth + 1);
        return;
    }
    if (const auto* assign = dynamic_cast<const AssignStmt*>(&stmt)) {
        out << indent(depth) << "Assign(" << assign->name << ")\n";
        dumpExpression(*assign->value, out, depth + 1);
        return;
    }
    if (const auto* ret = dynamic_cast<const ReturnStmt*>(&stmt)) {
        out << indent(depth) << "Return\n";
        if (ret->value != nullptr) {
            dumpExpression(*ret->value, out, depth + 1);
        }
        return;
    }
    if (dynamic_cast<const BreakStmt*>(&stmt) != nullptr) {
        out << indent(depth) << "Break\n";
        return;
    }
    if (dynamic_cast<const ContinueStmt*>(&stmt) != nullptr) {
        out << indent(depth) << "Continue\n";
        return;
    }
    if (const auto* expr = dynamic_cast<const ExprStmt*>(&stmt)) {
        out << indent(depth) << "ExprStmt\n";
        dumpExpression(*expr->expr, out, depth + 1);
        return;
    }
    if (const auto* if_stmt = dynamic_cast<const IfStmt*>(&stmt)) {
        out << indent(depth) << "If\n";
        dumpExpression(*if_stmt->condition, out, depth + 1);
        out << indent(depth + 1) << "Then\n";
        for (const auto& item : if_stmt->then_body) {
            dumpStatement(*item, out, depth + 2);
        }
        if (!if_stmt->else_body.empty()) {
            out << indent(depth + 1) << "Else\n";
            for (const auto& item : if_stmt->else_body) {
                dumpStatement(*item, out, depth + 2);
            }
        }
        return;
    }
    if (const auto* while_stmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        out << indent(depth) << "While\n";
        dumpExpression(*while_stmt->condition, out, depth + 1);
        for (const auto& item : while_stmt->body) {
            dumpStatement(*item, out, depth + 1);
        }
        return;
    }
    if (const auto* for_stmt = dynamic_cast<const ForStmt*>(&stmt)) {
        out << indent(depth) << "For(" << for_stmt->variable_name << ")\n";
        dumpExpression(*for_stmt->start, out, depth + 1);
        dumpExpression(*for_stmt->end, out, depth + 1);
        dumpExpression(*for_stmt->step, out, depth + 1);
        for (const auto& item : for_stmt->body) {
            dumpStatement(*item, out, depth + 1);
        }
        return;
    }
    if (const auto* repeat_stmt = dynamic_cast<const RepeatStmt*>(&stmt)) {
        out << indent(depth) << "Repeat\n";
        dumpExpression(*repeat_stmt->count, out, depth + 1);
        for (const auto& item : repeat_stmt->body) {
            dumpStatement(*item, out, depth + 1);
        }
        return;
    }
    if (const auto* asm_stmt = dynamic_cast<const InlineAsmStmt*>(&stmt)) {
        out << indent(depth) << "InlineAsm\n";
        if (!asm_stmt->operands.empty()) {
            out << indent(depth + 1) << "Operands\n";
            for (const auto& operand : asm_stmt->operands) {
                dumpExpression(*operand, out, depth + 2);
            }
        }
        for (const auto& line : asm_stmt->lines) {
            out << indent(depth + 1) << '"' << line << "\"\n";
        }
        return;
    }
    out << indent(depth) << "<unknown stmt>\n";
}

std::string dumpAst(const Program& program) {
    std::ostringstream out;
    for (const auto& decl : program.declarations) {
        if (const auto* external = dynamic_cast<const ExternFunctionDecl*>(decl.get())) {
            out << "ExternFunction(" << external->name << ") -> "
                << toString(external->return_type == ValueType::Unknown ? ValueType::Int64 : external->return_type) << '\n';
            for (const auto& parameter : external->parameters) {
                out << "  Param(" << parameter.name << ": " << toString(parameter.type == ValueType::Unknown ? ValueType::Int64 : parameter.type)
                    << ")\n";
            }
            continue;
        }
        const auto* function = dynamic_cast<const FunctionDecl*>(decl.get());
        out << "Function(" << function->name << ") -> " << toString(function->return_type) << '\n';
        for (const auto& parameter : function->parameters) {
            out << "  Param(" << parameter.name << ": " << toString(parameter.type == ValueType::Unknown ? ValueType::Int64 : parameter.type) << ")\n";
        }
        for (const auto& statement : function->body) {
            dumpStatement(*statement, out, 1);
        }
    }
    return out.str();
}

}  // namespace

BuildResult Compiler::build(const BuildOptions& options) const {
    try {
        const auto source = SourceFile::load(options.input_path);
        Diagnostics diagnostics(source);

        Lexer lexer(source, diagnostics);
        const auto tokens = lexer.tokenize();

        Parser parser(tokens, diagnostics);
        auto program = parser.parse();

        SemanticModel semantic;
        SemanticAnalyzer analyzer(diagnostics, semantic);
        analyzer.analyze(*program);

        Lowerer lowerer(semantic, LoweringOptions {.opt_level = options.opt_level});
        auto module = lowerer.lower(*program);

        Optimizer optimizer(options.opt_level);
        optimizer.run(module);

        BuildResult result;
        result.ast_dump = dumpAst(*program);
        result.ir_dump = dumpIR(module);

        Emitter emitter(source, EmitOptions {.target = options.target, .annotate_source = options.annotate_source});
        result.assembly = emitter.emit(module);
        return result;
    } catch (const CompilationError& error) {
        return BuildResult {.assembly = {}, .diagnostics = error.what(), .ir_dump = {}, .ast_dump = {}};
    } catch (const std::exception& error) {
        return BuildResult {.assembly = {}, .diagnostics = error.what(), .ir_dump = {}, .ast_dump = {}};
    }
}

}  // namespace zenasm

#include "zenasm/ast.hpp"
#include "zenasm/ir.hpp"
#include "zenasm/token.hpp"

namespace zenasm {

const char* toString(const TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::EndOfFile:
            return "eof";
        case TokenKind::Newline:
            return "newline";
        case TokenKind::Indent:
            return "indent";
        case TokenKind::Dedent:
            return "dedent";
        case TokenKind::Identifier:
            return "identifier";
        case TokenKind::Integer:
            return "integer";
        case TokenKind::String:
            return "string";
        case TokenKind::LeftParen:
            return "(";
        case TokenKind::RightParen:
            return ")";
        case TokenKind::Comma:
            return ",";
        case TokenKind::Colon:
            return ":";
        case TokenKind::Arrow:
            return "->";
        case TokenKind::Plus:
            return "+";
        case TokenKind::Minus:
            return "-";
        case TokenKind::Star:
            return "*";
        case TokenKind::Slash:
            return "/";
        case TokenKind::Percent:
            return "%";
        case TokenKind::Equal:
            return "=";
        case TokenKind::EqualEqual:
            return "==";
        case TokenKind::BangEqual:
            return "!=";
        case TokenKind::Less:
            return "<";
        case TokenKind::LessEqual:
            return "<=";
        case TokenKind::Greater:
            return ">";
        case TokenKind::GreaterEqual:
            return ">=";
        case TokenKind::KeywordFn:
            return "fn";
        case TokenKind::KeywordExtern:
            return "extern";
        case TokenKind::KeywordLet:
            return "let";
        case TokenKind::KeywordReturn:
            return "return";
        case TokenKind::KeywordIf:
            return "if";
        case TokenKind::KeywordElif:
            return "elif";
        case TokenKind::KeywordElse:
            return "else";
        case TokenKind::KeywordWhile:
            return "while";
        case TokenKind::KeywordFor:
            return "for";
        case TokenKind::KeywordIn:
            return "in";
        case TokenKind::KeywordRepeat:
            return "repeat";
        case TokenKind::KeywordBreak:
            return "break";
        case TokenKind::KeywordContinue:
            return "continue";
        case TokenKind::KeywordTrue:
            return "true";
        case TokenKind::KeywordFalse:
            return "false";
        case TokenKind::KeywordAnd:
            return "and";
        case TokenKind::KeywordOr:
            return "or";
        case TokenKind::KeywordNot:
            return "not";
        case TokenKind::KeywordAsm:
            return "asm";
    }
    return "?";
}

std::string toString(const ValueType type) {
    switch (type) {
        case ValueType::Unknown:
            return "unknown";
        case ValueType::Void:
            return "void";
        case ValueType::Int64:
            return "i64";
        case ValueType::Bool:
            return "bool";
        case ValueType::String:
            return "str";
    }
    return "unknown";
}

std::string toString(const UnaryOp op) {
    switch (op) {
        case UnaryOp::Negate:
            return "-";
        case UnaryOp::LogicalNot:
            return "not";
    }
    return "?";
}

std::string toString(const BinaryOp op) {
    switch (op) {
        case BinaryOp::Add:
            return "+";
        case BinaryOp::Sub:
            return "-";
        case BinaryOp::Mul:
            return "*";
        case BinaryOp::Div:
            return "/";
        case BinaryOp::Mod:
            return "%";
        case BinaryOp::Equal:
            return "==";
        case BinaryOp::NotEqual:
            return "!=";
        case BinaryOp::Less:
            return "<";
        case BinaryOp::LessEqual:
            return "<=";
        case BinaryOp::Greater:
            return ">";
        case BinaryOp::GreaterEqual:
            return ">=";
        case BinaryOp::LogicalAnd:
            return "and";
        case BinaryOp::LogicalOr:
            return "or";
    }
    return "?";
}

std::optional<VirtualRegister> defOf(const IRInstruction& instruction) {
    return std::visit(
        [](const auto& value) -> std::optional<VirtualRegister> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, IRLoadImm> || std::is_same_v<T, IRLoadAddress> || std::is_same_v<T, IRLoadLocal> ||
                          std::is_same_v<T, IRBinary> || std::is_same_v<T, IRUnary>) {
                return value.dest;
            } else if constexpr (std::is_same_v<T, IRCall>) {
                return value.dest;
            } else {
                return std::nullopt;
            }
        },
        instruction.data);
}

std::vector<VirtualRegister> usesOf(const IRInstruction& instruction) {
    return std::visit(
        [](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, IRStoreLocal>) {
                return std::vector<VirtualRegister> {value.value};
            } else if constexpr (std::is_same_v<T, IRBinary>) {
                return std::vector<VirtualRegister> {value.lhs, value.rhs};
            } else if constexpr (std::is_same_v<T, IRUnary>) {
                return std::vector<VirtualRegister> {value.value};
            } else if constexpr (std::is_same_v<T, IRCall>) {
                return value.arguments;
            } else if constexpr (std::is_same_v<T, IRPrint>) {
                return std::vector<VirtualRegister> {value.value};
            } else if constexpr (std::is_same_v<T, IRBranch>) {
                return std::vector<VirtualRegister> {value.condition};
            } else if constexpr (std::is_same_v<T, IRReturn>) {
                if (value.value.has_value()) {
                    return std::vector<VirtualRegister> {*value.value};
                }
                return std::vector<VirtualRegister> {};
            } else if constexpr (std::is_same_v<T, IRInlineAsm>) {
                std::vector<VirtualRegister> regs;
                for (const auto& operand : value.operands) {
                    if (operand.value.has_value()) {
                        regs.push_back(*operand.value);
                    }
                }
                return regs;
            } else {
                return std::vector<VirtualRegister> {};
            }
        },
        instruction.data);
}

bool hasSideEffects(const IRInstruction& instruction) {
    return std::visit(
        [](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            return std::is_same_v<T, IRStoreLocal> || std::is_same_v<T, IRCall> || std::is_same_v<T, IRPrint> ||
                   std::is_same_v<T, IRBranch> || std::is_same_v<T, IRJump> || std::is_same_v<T, IRReturn> ||
                   std::is_same_v<T, IRInlineAsm>;
        },
        instruction.data);
}

bool isTerminator(const IRInstruction& instruction) {
    return std::holds_alternative<IRBranch>(instruction.data) || std::holds_alternative<IRJump>(instruction.data) ||
           std::holds_alternative<IRReturn>(instruction.data);
}

std::vector<BlockId> successorsOf(const IRBasicBlock& block) {
    if (block.instructions.empty()) {
        return {};
    }
    const auto& terminator = block.instructions.back();
    if (const auto* branch = std::get_if<IRBranch>(&terminator.data)) {
        return {branch->true_block, branch->false_block};
    }
    if (const auto* jump = std::get_if<IRJump>(&terminator.data)) {
        return {jump->target};
    }
    return {};
}

std::string toString(const IROpcode opcode) {
    switch (opcode) {
        case IROpcode::LoadImm:
            return "load_imm";
        case IROpcode::LoadAddress:
            return "load_address";
        case IROpcode::LoadLocal:
            return "load_local";
        case IROpcode::StoreLocal:
            return "store_local";
        case IROpcode::Add:
            return "add";
        case IROpcode::Sub:
            return "sub";
        case IROpcode::Mul:
            return "mul";
        case IROpcode::Div:
            return "div";
        case IROpcode::Mod:
            return "mod";
        case IROpcode::CmpEq:
            return "cmp_eq";
        case IROpcode::CmpNe:
            return "cmp_ne";
        case IROpcode::CmpLt:
            return "cmp_lt";
        case IROpcode::CmpLe:
            return "cmp_le";
        case IROpcode::CmpGt:
            return "cmp_gt";
        case IROpcode::CmpGe:
            return "cmp_ge";
        case IROpcode::LogicalAnd:
            return "and";
        case IROpcode::LogicalOr:
            return "or";
        case IROpcode::Neg:
            return "neg";
        case IROpcode::Not:
            return "not";
        case IROpcode::Call:
            return "call";
        case IROpcode::Print:
            return "print";
        case IROpcode::Branch:
            return "branch";
        case IROpcode::Jump:
            return "jump";
        case IROpcode::Return:
            return "return";
        case IROpcode::InlineAsm:
            return "inline_asm";
    }
    return "unknown";
}

}  // namespace zenasm

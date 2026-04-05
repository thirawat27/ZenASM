#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "zenasm/ast.hpp"

namespace zenasm {

using VirtualRegister = int;
using LocalSlot = int;
using BlockId = int;

enum class IROpcode {
    LoadImm,
    LoadAddress,
    LoadLocal,
    StoreLocal,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    CmpEq,
    CmpNe,
    CmpLt,
    CmpLe,
    CmpGt,
    CmpGe,
    LogicalAnd,
    LogicalOr,
    Neg,
    Not,
    Call,
    Print,
    Branch,
    Jump,
    Return,
    InlineAsm,
};

struct IRLoadImm {
    VirtualRegister dest = -1;
    std::int64_t value = 0;
};

struct IRLoadAddress {
    VirtualRegister dest = -1;
    std::string symbol;
};

struct IRLoadLocal {
    VirtualRegister dest = -1;
    LocalSlot local = -1;
};

struct IRStoreLocal {
    LocalSlot local = -1;
    VirtualRegister value = -1;
};

struct IRBinary {
    IROpcode opcode = IROpcode::Add;
    VirtualRegister dest = -1;
    VirtualRegister lhs = -1;
    VirtualRegister rhs = -1;
};

struct IRUnary {
    IROpcode opcode = IROpcode::Neg;
    VirtualRegister dest = -1;
    VirtualRegister value = -1;
};

struct IRCall {
    std::optional<VirtualRegister> dest;
    std::string callee;
    std::vector<VirtualRegister> arguments;
};

struct IRPrint {
    VirtualRegister value = -1;
    ValueType type = ValueType::Int64;
};

struct IRBranch {
    VirtualRegister condition = -1;
    BlockId true_block = -1;
    BlockId false_block = -1;
};

struct IRJump {
    BlockId target = -1;
};

struct IRReturn {
    std::optional<VirtualRegister> value;
};

struct IRInlineAsmOperand {
    std::optional<VirtualRegister> value;
    std::optional<LocalSlot> local;
    ValueType type = ValueType::Unknown;
};

struct IRInlineAsm {
    std::vector<std::string> lines;
    std::vector<IRInlineAsmOperand> operands;
};

using IRInstructionData =
    std::variant<IRLoadImm, IRLoadAddress, IRLoadLocal, IRStoreLocal, IRBinary, IRUnary, IRCall, IRPrint, IRBranch, IRJump, IRReturn, IRInlineAsm>;

struct IRInstruction {
    IRInstructionData data;
    SourceSpan span {};
};

struct IRBasicBlock {
    BlockId id = -1;
    std::string label;
    std::vector<IRInstruction> instructions;
};

struct IRFunction {
    std::string name;
    bool is_extern = false;
    bool contains_calls = false;
    ValueType return_type = ValueType::Void;
    std::vector<ValueType> parameter_types;
    std::vector<std::string> parameter_names;
    std::vector<ValueType> local_types;
    std::vector<std::string> local_names;
    std::vector<IRBasicBlock> blocks;
    BlockId entry = -1;
    int next_vreg = 0;
};

struct IRModule {
    struct GlobalData {
        std::string label;
        std::string bytes;
    };

    std::vector<GlobalData> globals;
    std::vector<IRFunction> functions;
};

[[nodiscard]] std::optional<VirtualRegister> defOf(const IRInstruction& instruction);
[[nodiscard]] std::vector<VirtualRegister> usesOf(const IRInstruction& instruction);
[[nodiscard]] bool hasSideEffects(const IRInstruction& instruction);
[[nodiscard]] bool isTerminator(const IRInstruction& instruction);
[[nodiscard]] std::vector<BlockId> successorsOf(const IRBasicBlock& block);
[[nodiscard]] std::string toString(IROpcode opcode);

}  // namespace zenasm

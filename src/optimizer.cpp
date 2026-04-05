#include "zenasm/optimizer.hpp"

#include <functional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace zenasm {

namespace {

std::optional<std::int64_t> constantFor(const std::unordered_map<VirtualRegister, std::int64_t>& constants, const VirtualRegister reg) {
    const auto found = constants.find(reg);
    if (found == constants.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<std::int64_t> foldBinary(const IROpcode opcode, const std::int64_t lhs, const std::int64_t rhs) {
    switch (opcode) {
        case IROpcode::Add:
            return lhs + rhs;
        case IROpcode::Sub:
            return lhs - rhs;
        case IROpcode::Mul:
            return lhs * rhs;
        case IROpcode::Div:
            if (rhs == 0) {
                return std::nullopt;
            }
            return lhs / rhs;
        case IROpcode::Mod:
            if (rhs == 0) {
                return std::nullopt;
            }
            return lhs % rhs;
        case IROpcode::CmpEq:
            return lhs == rhs ? 1 : 0;
        case IROpcode::CmpNe:
            return lhs != rhs ? 1 : 0;
        case IROpcode::CmpLt:
            return lhs < rhs ? 1 : 0;
        case IROpcode::CmpLe:
            return lhs <= rhs ? 1 : 0;
        case IROpcode::CmpGt:
            return lhs > rhs ? 1 : 0;
        case IROpcode::CmpGe:
            return lhs >= rhs ? 1 : 0;
        case IROpcode::LogicalAnd:
            return (lhs != 0 && rhs != 0) ? 1 : 0;
        case IROpcode::LogicalOr:
            return (lhs != 0 || rhs != 0) ? 1 : 0;
        default:
            return std::nullopt;
    }
}

std::optional<std::int64_t> foldUnary(const IROpcode opcode, const std::int64_t value) {
    switch (opcode) {
        case IROpcode::Neg:
            return -value;
        case IROpcode::Not:
            return value == 0 ? 1 : 0;
        default:
            return std::nullopt;
    }
}

std::string dumpInstruction(const IRInstruction& instruction) {
    return std::visit(
        [](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            std::ostringstream out;
            if constexpr (std::is_same_v<T, IRLoadImm>) {
                out << '%' << value.dest << " = load_imm " << value.value;
            } else if constexpr (std::is_same_v<T, IRLoadAddress>) {
                out << '%' << value.dest << " = load_address " << value.symbol;
            } else if constexpr (std::is_same_v<T, IRLoadLocal>) {
                out << '%' << value.dest << " = load_local $" << value.local;
            } else if constexpr (std::is_same_v<T, IRStoreLocal>) {
                out << "store_local $" << value.local << ", %" << value.value;
            } else if constexpr (std::is_same_v<T, IRBinary>) {
                out << '%' << value.dest << " = " << toString(value.opcode) << " %" << value.lhs << ", %" << value.rhs;
            } else if constexpr (std::is_same_v<T, IRUnary>) {
                out << '%' << value.dest << " = " << toString(value.opcode) << " %" << value.value;
            } else if constexpr (std::is_same_v<T, IRCall>) {
                if (value.dest.has_value()) {
                    out << '%' << *value.dest << " = ";
                }
                out << "call " << value.callee << '(';
                for (std::size_t index = 0; index < value.arguments.size(); ++index) {
                    if (index != 0) {
                        out << ", ";
                    }
                    out << '%' << value.arguments[index];
                }
                out << ')';
            } else if constexpr (std::is_same_v<T, IRPrint>) {
                out << "print." << toString(value.type) << " %" << value.value;
            } else if constexpr (std::is_same_v<T, IRBranch>) {
                out << "branch %" << value.condition << " ? block" << value.true_block << " : block" << value.false_block;
            } else if constexpr (std::is_same_v<T, IRJump>) {
                out << "jump block" << value.target;
            } else if constexpr (std::is_same_v<T, IRReturn>) {
                out << "return";
                if (value.value.has_value()) {
                    out << " %" << *value.value;
                }
            } else if constexpr (std::is_same_v<T, IRInlineAsm>) {
                out << "inline_asm[" << value.lines.size() << " line(s), " << value.operands.size() << " operand(s)]";
            }
            return out.str();
        },
        instruction.data);
}

}  // namespace

Optimizer::Optimizer(const int opt_level)
    : opt_level_(opt_level) {}

void Optimizer::run(IRModule& module) const {
    for (auto& function : module.functions) {
        if (function.is_extern) {
            continue;
        }
        optimizeFunction(function);
    }
}

void Optimizer::optimizeFunction(IRFunction& function) const {
    if (opt_level_ <= 0) {
        return;
    }

    foldConstants(function);
    simplifyBranches(function);
    removeUnreachableBlocks(function);
    eliminateDeadStores(function);
    eliminateDeadCode(function);

    if (opt_level_ >= 2) {
        foldConstants(function);
        simplifyBranches(function);
        removeUnreachableBlocks(function);
        eliminateDeadStores(function);
        eliminateDeadCode(function);
    }
}

void Optimizer::foldConstants(IRFunction& function) const {
    for (auto& block : function.blocks) {
        std::unordered_map<VirtualRegister, std::int64_t> vreg_constants;
        std::unordered_map<LocalSlot, std::int64_t> local_constants;

        for (auto& instruction : block.instructions) {
            if (auto* load_imm = std::get_if<IRLoadImm>(&instruction.data)) {
                vreg_constants[load_imm->dest] = load_imm->value;
                continue;
            }

            if (auto* load_local = std::get_if<IRLoadLocal>(&instruction.data)) {
                const auto dest = load_local->dest;
                const auto local = load_local->local;
                const auto local_constant = local_constants.find(local);
                if (local_constant != local_constants.end()) {
                    instruction.data = IRLoadImm {.dest = dest, .value = local_constant->second};
                    vreg_constants[dest] = local_constant->second;
                } else {
                    vreg_constants.erase(dest);
                }
                continue;
            }

            if (auto* store_local = std::get_if<IRStoreLocal>(&instruction.data)) {
                const auto value_constant = constantFor(vreg_constants, store_local->value);
                if (value_constant.has_value()) {
                    local_constants[store_local->local] = *value_constant;
                } else {
                    local_constants.erase(store_local->local);
                }
                continue;
            }

            if (auto* binary = std::get_if<IRBinary>(&instruction.data)) {
                const auto dest = binary->dest;
                const auto opcode = binary->opcode;
                const auto lhs_reg = binary->lhs;
                const auto rhs_reg = binary->rhs;
                const auto lhs = constantFor(vreg_constants, lhs_reg);
                const auto rhs = constantFor(vreg_constants, rhs_reg);
                if (lhs.has_value() && rhs.has_value()) {
                    const auto folded = foldBinary(opcode, *lhs, *rhs);
                    if (folded.has_value()) {
                        instruction.data = IRLoadImm {.dest = dest, .value = *folded};
                        vreg_constants[dest] = *folded;
                        continue;
                    }
                }
                vreg_constants.erase(dest);
                continue;
            }

            if (auto* unary = std::get_if<IRUnary>(&instruction.data)) {
                const auto dest = unary->dest;
                const auto opcode = unary->opcode;
                const auto source = unary->value;
                const auto operand = constantFor(vreg_constants, source);
                if (operand.has_value()) {
                    const auto folded = foldUnary(opcode, *operand);
                    if (folded.has_value()) {
                        instruction.data = IRLoadImm {.dest = dest, .value = *folded};
                        vreg_constants[dest] = *folded;
                        continue;
                    }
                }
                vreg_constants.erase(dest);
                continue;
            }

            if (const auto* call = std::get_if<IRCall>(&instruction.data)) {
                if (call->dest.has_value()) {
                    vreg_constants.erase(*call->dest);
                }
                continue;
            }
        }
    }
}

void Optimizer::simplifyBranches(IRFunction& function) const {
    for (auto& block : function.blocks) {
        std::unordered_map<VirtualRegister, std::int64_t> constants;
        for (auto& instruction : block.instructions) {
            if (auto* load_imm = std::get_if<IRLoadImm>(&instruction.data)) {
                constants[load_imm->dest] = load_imm->value;
                continue;
            }

            if (auto* branch = std::get_if<IRBranch>(&instruction.data)) {
                const auto condition = constantFor(constants, branch->condition);
                if (condition.has_value()) {
                    instruction.data = IRJump {.target = *condition != 0 ? branch->true_block : branch->false_block};
                }
                break;
            }

            if (const auto defined = defOf(instruction); defined.has_value()) {
                constants.erase(*defined);
            }
        }
    }
}

void Optimizer::eliminateDeadCode(IRFunction& function) const {
    bool changed = true;
    while (changed) {
        changed = false;
        std::unordered_map<VirtualRegister, int> use_counts;
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                for (const auto reg : usesOf(instruction)) {
                    ++use_counts[reg];
                }
            }
        }

        for (auto& block : function.blocks) {
            std::vector<IRInstruction> retained;
            retained.reserve(block.instructions.size());
            for (const auto& instruction : block.instructions) {
                const auto def = defOf(instruction);
                if (def.has_value() && use_counts[*def] == 0 && !hasSideEffects(instruction)) {
                    changed = true;
                    continue;
                }
                retained.push_back(instruction);
            }
            block.instructions = std::move(retained);
        }
    }
}

void Optimizer::eliminateDeadStores(IRFunction& function) const {
    std::unordered_map<BlockId, std::unordered_set<LocalSlot>> use_sets;
    std::unordered_map<BlockId, std::unordered_set<LocalSlot>> def_sets;
    std::unordered_map<BlockId, std::unordered_set<LocalSlot>> live_in;
    std::unordered_map<BlockId, std::unordered_set<LocalSlot>> live_out;

    for (const auto& block : function.blocks) {
        auto& use = use_sets[block.id];
        auto& def = def_sets[block.id];
        for (const auto& instruction : block.instructions) {
            if (const auto* load = std::get_if<IRLoadLocal>(&instruction.data)) {
                if (!def.contains(load->local)) {
                    use.insert(load->local);
                }
            } else if (const auto* store = std::get_if<IRStoreLocal>(&instruction.data)) {
                def.insert(store->local);
            } else if (std::holds_alternative<IRInlineAsm>(instruction.data)) {
                for (LocalSlot slot = 0; slot < static_cast<LocalSlot>(function.local_types.size()); ++slot) {
                    use.insert(slot);
                }
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto block_it = function.blocks.rbegin(); block_it != function.blocks.rend(); ++block_it) {
            const auto& block = *block_it;
            auto new_out = std::unordered_set<LocalSlot> {};
            for (const auto successor : successorsOf(block)) {
                const auto& successor_live_in = live_in[successor];
                new_out.insert(successor_live_in.begin(), successor_live_in.end());
            }

            auto new_in = use_sets[block.id];
            for (const auto local : new_out) {
                if (!def_sets[block.id].contains(local)) {
                    new_in.insert(local);
                }
            }

            if (new_out != live_out[block.id] || new_in != live_in[block.id]) {
                live_out[block.id] = std::move(new_out);
                live_in[block.id] = std::move(new_in);
                changed = true;
            }
        }
    }

    for (auto& block : function.blocks) {
        auto live = live_out[block.id];
        std::vector<IRInstruction> rewritten;
        rewritten.reserve(block.instructions.size());
        for (auto instruction_it = block.instructions.rbegin(); instruction_it != block.instructions.rend(); ++instruction_it) {
            if (const auto* load = std::get_if<IRLoadLocal>(&instruction_it->data)) {
                live.insert(load->local);
                rewritten.push_back(*instruction_it);
                continue;
            }
            if (const auto* store = std::get_if<IRStoreLocal>(&instruction_it->data)) {
                if (!live.contains(store->local)) {
                    continue;
                }
                live.erase(store->local);
                rewritten.push_back(*instruction_it);
                continue;
            }
            if (std::holds_alternative<IRInlineAsm>(instruction_it->data)) {
                for (LocalSlot slot = 0; slot < static_cast<LocalSlot>(function.local_types.size()); ++slot) {
                    live.insert(slot);
                }
            }
            rewritten.push_back(*instruction_it);
        }
        std::reverse(rewritten.begin(), rewritten.end());
        block.instructions = std::move(rewritten);
    }
}

void Optimizer::removeUnreachableBlocks(IRFunction& function) const {
    if (function.entry < 0 || function.blocks.empty()) {
        return;
    }

    std::unordered_set<BlockId> reachable;
    std::function<void(BlockId)> visit = [&](const BlockId id) {
        if (!reachable.insert(id).second) {
            return;
        }
        for (const auto successor : successorsOf(function.blocks[static_cast<std::size_t>(id)])) {
            visit(successor);
        }
    };
    visit(function.entry);

    std::unordered_map<BlockId, BlockId> remap;
    std::vector<IRBasicBlock> new_blocks;
    new_blocks.reserve(reachable.size());
    for (const auto& block : function.blocks) {
        if (!reachable.contains(block.id)) {
            continue;
        }
        const BlockId new_id = static_cast<BlockId>(new_blocks.size());
        remap[block.id] = new_id;
        new_blocks.push_back(block);
        new_blocks.back().id = new_id;
    }

    for (auto& block : new_blocks) {
        for (auto& instruction : block.instructions) {
            if (auto* branch = std::get_if<IRBranch>(&instruction.data)) {
                branch->true_block = remap.at(branch->true_block);
                branch->false_block = remap.at(branch->false_block);
            } else if (auto* jump = std::get_if<IRJump>(&instruction.data)) {
                jump->target = remap.at(jump->target);
            }
        }
    }

    function.entry = remap.at(function.entry);
    function.blocks = std::move(new_blocks);
}

std::string dumpIR(const IRModule& module) {
    std::ostringstream output;
    for (const auto& function : module.functions) {
        if (function.is_extern) {
            output << "extern fn " << function.name << '(';
            for (std::size_t index = 0; index < function.parameter_types.size(); ++index) {
                if (index != 0) {
                    output << ", ";
                }
                output << toString(function.parameter_types[index]);
            }
            output << ") -> " << toString(function.return_type) << "\n\n";
            continue;
        }

        output << "fn " << function.name << '(';
        for (std::size_t index = 0; index < function.parameter_types.size(); ++index) {
            if (index != 0) {
                output << ", ";
            }
            output << function.parameter_names[index] << ": " << toString(function.parameter_types[index]);
        }
        output << ") -> " << toString(function.return_type) << '\n';
        for (const auto& block : function.blocks) {
            output << "  block" << block.id << " (" << block.label << ")\n";
            for (const auto& instruction : block.instructions) {
                output << "    " << dumpInstruction(instruction) << '\n';
            }
        }
        output << '\n';
    }
    return output.str();
}

}  // namespace zenasm

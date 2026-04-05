#include "zenasm/emitter.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <unordered_set>

namespace zenasm {

namespace {

struct FrameLayout {
    int saved_register_bytes = 0;
    int local_slot_bytes = 0;
    int spill_slot_bytes = 0;
    int shadow_space_bytes = 0;
    int frame_size = 0;
};

FrameLayout computeFrameLayout(const IRFunction& function, const FunctionAllocation& allocation, const TargetPlatform target) {
    FrameLayout layout;
    layout.saved_register_bytes = static_cast<int>(allocation.used_registers.size() * 8);
    layout.local_slot_bytes = static_cast<int>(function.local_types.size() * 8);
    layout.spill_slot_bytes = allocation.spill_count * 8;
    layout.shadow_space_bytes = (target == TargetPlatform::Win64 && function.contains_calls) ? 32 : 0;
    const int raw = layout.local_slot_bytes + layout.spill_slot_bytes + layout.shadow_space_bytes;
    layout.frame_size = raw;
    const int required_mod = layout.saved_register_bytes % 16;
    while (layout.frame_size % 16 != required_mod) {
        layout.frame_size += 8;
    }
    return layout;
}

std::vector<std::string> callArgumentRegisters(const TargetPlatform target) {
    if (target == TargetPlatform::Win64) {
        return {"rcx", "rdx", "r8", "r9"};
    }
    return {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
}

int incomingStackArgumentOffset(const TargetPlatform target, const int argument_index) {
    if (target == TargetPlatform::Win64) {
        if (argument_index < 4) {
            return -1;
        }
        return 48 + ((argument_index - 4) * 8);
    }
    if (argument_index < 6) {
        return -1;
    }
    return 16 + ((argument_index - 6) * 8);
}

std::string sanitizeComment(std::string_view text) {
    std::string value(text);
    for (auto& ch : value) {
        if (ch == '\r' || ch == '\n') {
            ch = ' ';
        }
    }
    return value;
}

std::string escapeAsmString(std::string_view bytes) {
    std::ostringstream out;
    for (const unsigned char ch : bytes) {
        switch (ch) {
            case '\n':
                out << "\\n";
                break;
            case '\t':
                out << "\\t";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            default:
                if (ch < 32 || ch >= 127) {
                    out << '\\' << std::oct << static_cast<int>(ch) << std::dec;
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    return out.str();
}

}  // namespace

Emitter::Emitter(const SourceFile& source, const EmitOptions options)
    : source_(source),
      options_(options) {}

std::string Emitter::emit(const IRModule& module) const {
    std::ostringstream out;
    out << ".intel_syntax noprefix\n";

    bool uses_printf = false;
    bool uses_puts = false;
    std::unordered_set<std::string> external_symbols;
    for (const auto& function : module.functions) {
        if (function.is_extern) {
            external_symbols.insert(function.name);
            continue;
        }
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                if (std::holds_alternative<IRPrint>(instruction.data)) {
                    if (std::get<IRPrint>(instruction.data).type == ValueType::String) {
                        uses_puts = true;
                    } else {
                        uses_printf = true;
                    }
                }
            }
        }
    }

    if (uses_printf) {
        external_symbols.insert("printf");
    }
    if (uses_puts) {
        external_symbols.insert("puts");
    }

    for (const auto& symbol : external_symbols) {
        out << ".extern " << symbol << '\n';
    }
    if (!external_symbols.empty()) {
        out << '\n';
    }

    if (uses_printf || !module.globals.empty()) {
        if (options_.target == TargetPlatform::Win64) {
            out << ".section .rdata,\"dr\"\n";
        } else {
            out << ".section .rodata\n";
        }
        if (uses_printf) {
            out << "zenasm_fmt_i64:\n";
            out << "    .asciz \"%lld\\n\"\n";
        }
        for (const auto& global : module.globals) {
            out << global.label << ":\n";
            out << "    .asciz \"" << escapeAsmString(global.bytes) << "\"\n";
        }
        out << '\n';
    }

    out << ".text\n";

    RegisterAllocator allocator;
    for (const auto& function : module.functions) {
        if (function.is_extern) {
            continue;
        }

        const auto allocation = allocator.allocate(function);
        const auto layout = computeFrameLayout(function, allocation, options_.target);
        const auto arg_regs = callArgumentRegisters(options_.target);
        const std::string exit_label = function.name + ".exit";

        const auto localOffset = [&](const LocalSlot slot) {
            return layout.saved_register_bytes + ((slot + 1) * 8);
        };
        const auto spillOffset = [&](const int spill_slot) {
            return layout.saved_register_bytes + layout.local_slot_bytes + ((spill_slot + 1) * 8);
        };
        const auto stackOperand = [&](const int offset) {
            return "qword ptr [rbp - " + std::to_string(offset) + "]";
        };
        const auto locationOf = [&](const VirtualRegister reg) -> AllocatedLocation {
            const auto found = allocation.locations.find(reg);
            if (found == allocation.locations.end()) {
                return AllocatedLocation {.spilled = true, .reg = PhysReg::Rbx, .spill_slot = allocation.spill_count};
            }
            return found->second;
        };
        const auto loadInto = [&](std::ostringstream& buffer, const std::string& target, const VirtualRegister reg) {
            const auto location = locationOf(reg);
            if (location.spilled) {
                buffer << "    mov " << target << ", " << stackOperand(spillOffset(location.spill_slot)) << '\n';
            } else if (registerName(location.reg) != target) {
                buffer << "    mov " << target << ", " << registerName(location.reg) << '\n';
            }
        };
        const auto operandReg = [&](std::ostringstream& buffer, const VirtualRegister reg, const std::string& scratch) {
            const auto location = locationOf(reg);
            if (location.spilled) {
                buffer << "    mov " << scratch << ", " << stackOperand(spillOffset(location.spill_slot)) << '\n';
                return scratch;
            }
            return registerName(location.reg);
        };
        const auto storeFrom = [&](std::ostringstream& buffer, const VirtualRegister reg, const std::string& source) {
            const auto location = locationOf(reg);
            if (location.spilled) {
                buffer << "    mov " << stackOperand(spillOffset(location.spill_slot)) << ", " << source << '\n';
            } else if (registerName(location.reg) != source) {
                buffer << "    mov " << registerName(location.reg) << ", " << source << '\n';
            }
        };

        out << ".globl " << function.name << '\n';
        out << function.name << ":\n";
        out << "    push rbp\n";
        out << "    mov rbp, rsp\n";
        for (const auto reg : allocation.used_registers) {
            out << "    push " << registerName(reg) << '\n';
        }
        if (layout.frame_size > 0) {
            out << "    sub rsp, " << layout.frame_size << '\n';
        }

        for (std::size_t index = 0; index < function.parameter_names.size(); ++index) {
            const auto target_slot = static_cast<LocalSlot>(index);
            if (static_cast<int>(index) < static_cast<int>(arg_regs.size())) {
                out << "    mov " << stackOperand(localOffset(target_slot)) << ", " << arg_regs[index] << '\n';
            } else {
                const int stack_offset = incomingStackArgumentOffset(options_.target, static_cast<int>(index));
                out << "    mov r10, qword ptr [rbp + " << stack_offset << "]\n";
                out << "    mov " << stackOperand(localOffset(target_slot)) << ", r10\n";
            }
        }

        int last_line = -1;
        for (const auto& block : function.blocks) {
            out << block.label << ":\n";
            for (const auto& instruction : block.instructions) {
                if (options_.annotate_source && instruction.span.begin.line > 0 && instruction.span.begin.line != last_line) {
                    last_line = instruction.span.begin.line;
                    const auto line = sanitizeComment(source_.lineText(last_line));
                    if (!line.empty()) {
                        out << "    # line " << last_line << ": " << line << '\n';
                    }
                }

                std::visit(
                    [&](const auto& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, IRLoadImm>) {
                            const auto location = locationOf(value.dest);
                            if (location.spilled) {
                                out << "    mov r10, " << value.value << '\n';
                                out << "    mov " << stackOperand(spillOffset(location.spill_slot)) << ", r10\n";
                            } else {
                                out << "    mov " << registerName(location.reg) << ", " << value.value << '\n';
                            }
                        } else if constexpr (std::is_same_v<T, IRLoadAddress>) {
                            const auto location = locationOf(value.dest);
                            if (location.spilled) {
                                out << "    lea r10, [rip + " << value.symbol << "]\n";
                                out << "    mov " << stackOperand(spillOffset(location.spill_slot)) << ", r10\n";
                            } else {
                                out << "    lea " << registerName(location.reg) << ", [rip + " << value.symbol << "]\n";
                            }
                        } else if constexpr (std::is_same_v<T, IRLoadLocal>) {
                            const auto location = locationOf(value.dest);
                            if (location.spilled) {
                                out << "    mov r10, " << stackOperand(localOffset(value.local)) << '\n';
                                out << "    mov " << stackOperand(spillOffset(location.spill_slot)) << ", r10\n";
                            } else {
                                out << "    mov " << registerName(location.reg) << ", " << stackOperand(localOffset(value.local)) << '\n';
                            }
                        } else if constexpr (std::is_same_v<T, IRStoreLocal>) {
                            const auto source = operandReg(out, value.value, "r10");
                            out << "    mov " << stackOperand(localOffset(value.local)) << ", " << source << '\n';
                        } else if constexpr (std::is_same_v<T, IRBinary>) {
                            if (value.opcode == IROpcode::Div || value.opcode == IROpcode::Mod) {
                                loadInto(out, "rax", value.lhs);
                                out << "    cqo\n";
                                const auto rhs = operandReg(out, value.rhs, "r10");
                                out << "    idiv " << rhs << '\n';
                                storeFrom(out, value.dest, value.opcode == IROpcode::Div ? "rax" : "rdx");
                                return;
                            }

                            loadInto(out, "r10", value.lhs);
                            loadInto(out, "r11", value.rhs);

                            switch (value.opcode) {
                                case IROpcode::Add:
                                    out << "    add r10, r11\n";
                                    break;
                                case IROpcode::Sub:
                                    out << "    sub r10, r11\n";
                                    break;
                                case IROpcode::Mul:
                                    out << "    imul r10, r11\n";
                                    break;
                                case IROpcode::LogicalAnd:
                                    out << "    and r10, r11\n";
                                    out << "    cmp r10, 0\n";
                                    out << "    setne al\n";
                                    out << "    movzx r10, al\n";
                                    break;
                                case IROpcode::LogicalOr:
                                    out << "    or r10, r11\n";
                                    out << "    cmp r10, 0\n";
                                    out << "    setne al\n";
                                    out << "    movzx r10, al\n";
                                    break;
                                case IROpcode::CmpEq:
                                case IROpcode::CmpNe:
                                case IROpcode::CmpLt:
                                case IROpcode::CmpLe:
                                case IROpcode::CmpGt:
                                case IROpcode::CmpGe:
                                    out << "    cmp r10, r11\n";
                                    switch (value.opcode) {
                                        case IROpcode::CmpEq:
                                            out << "    sete al\n";
                                            break;
                                        case IROpcode::CmpNe:
                                            out << "    setne al\n";
                                            break;
                                        case IROpcode::CmpLt:
                                            out << "    setl al\n";
                                            break;
                                        case IROpcode::CmpLe:
                                            out << "    setle al\n";
                                            break;
                                        case IROpcode::CmpGt:
                                            out << "    setg al\n";
                                            break;
                                        case IROpcode::CmpGe:
                                            out << "    setge al\n";
                                            break;
                                        default:
                                            break;
                                    }
                                    out << "    movzx r10, al\n";
                                    break;
                                default:
                                    break;
                            }

                            storeFrom(out, value.dest, "r10");
                        } else if constexpr (std::is_same_v<T, IRUnary>) {
                            loadInto(out, "r10", value.value);
                            if (value.opcode == IROpcode::Neg) {
                                out << "    neg r10\n";
                            } else {
                                out << "    cmp r10, 0\n";
                                out << "    sete al\n";
                                out << "    movzx r10, al\n";
                            }
                            storeFrom(out, value.dest, "r10");
                        } else if constexpr (std::is_same_v<T, IRCall>) {
                            const auto reg_limit = static_cast<int>(arg_regs.size());
                            const int extra_args = std::max(0, static_cast<int>(value.arguments.size()) - reg_limit);
                            int dynamic_call_space = 0;
                            int stack_base = 0;
                            if (extra_args > 0) {
                                if (options_.target == TargetPlatform::Win64) {
                                    dynamic_call_space = 32 + (extra_args * 8);
                                    while ((dynamic_call_space % 16) != 0) {
                                        dynamic_call_space += 8;
                                    }
                                    stack_base = 32;
                                } else {
                                    dynamic_call_space = extra_args * 8;
                                    while ((dynamic_call_space % 16) != 0) {
                                        dynamic_call_space += 8;
                                    }
                                    stack_base = 0;
                                }
                                out << "    sub rsp, " << dynamic_call_space << '\n';
                            }

                            for (int index = reg_limit; index < static_cast<int>(value.arguments.size()); ++index) {
                                const auto source = operandReg(out, value.arguments[static_cast<std::size_t>(index)], "r10");
                                out << "    mov qword ptr [rsp + " << (stack_base + ((index - reg_limit) * 8)) << "], " << source << '\n';
                            }
                            for (int index = 0; index < std::min(reg_limit, static_cast<int>(value.arguments.size())); ++index) {
                                loadInto(out, arg_regs[static_cast<std::size_t>(index)], value.arguments[static_cast<std::size_t>(index)]);
                            }
                            out << "    call " << value.callee << '\n';
                            if (dynamic_call_space > 0) {
                                out << "    add rsp, " << dynamic_call_space << '\n';
                            }
                            if (value.dest.has_value()) {
                                storeFrom(out, *value.dest, "rax");
                            }
                        } else if constexpr (std::is_same_v<T, IRPrint>) {
                            if (value.type == ValueType::String) {
                                loadInto(out, options_.target == TargetPlatform::Win64 ? "rcx" : "rdi", value.value);
                                out << "    call puts\n";
                            } else {
                                loadInto(out, options_.target == TargetPlatform::Win64 ? "rdx" : "rsi", value.value);
                                out << "    lea " << (options_.target == TargetPlatform::Win64 ? "rcx" : "rdi")
                                    << ", [rip + zenasm_fmt_i64]\n";
                                out << "    xor eax, eax\n";
                                out << "    call printf\n";
                            }
                        } else if constexpr (std::is_same_v<T, IRBranch>) {
                            loadInto(out, "r10", value.condition);
                            out << "    cmp r10, 0\n";
                            out << "    jne " << function.blocks[static_cast<std::size_t>(value.true_block)].label << '\n';
                            out << "    jmp " << function.blocks[static_cast<std::size_t>(value.false_block)].label << '\n';
                        } else if constexpr (std::is_same_v<T, IRJump>) {
                            out << "    jmp " << function.blocks[static_cast<std::size_t>(value.target)].label << '\n';
                        } else if constexpr (std::is_same_v<T, IRReturn>) {
                            if (value.value.has_value()) {
                                loadInto(out, "rax", *value.value);
                            } else {
                                out << "    xor eax, eax\n";
                            }
                            out << "    jmp " << exit_label << '\n';
                        } else if constexpr (std::is_same_v<T, IRInlineAsm>) {
                            const auto operandText = [&](const IRInlineAsmOperand& operand) {
                                if (operand.local.has_value()) {
                                    return stackOperand(localOffset(*operand.local));
                                }
                                if (!operand.value.has_value()) {
                                    throw std::runtime_error("inline asm operand has no bound value");
                                }
                                const auto location = locationOf(*operand.value);
                                if (location.spilled) {
                                    return stackOperand(spillOffset(location.spill_slot));
                                }
                                return registerName(location.reg);
                            };
                            const auto renderAsmLine = [&](const std::string& line) {
                                std::string rendered;
                                rendered.reserve(line.size() + 16);
                                for (std::size_t index = 0; index < line.size(); ++index) {
                                    if (line[index] == '{') {
                                        std::size_t end = index + 1;
                                        while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end])) != 0) {
                                            ++end;
                                        }
                                        if (end < line.size() && line[end] == '}' && end > index + 1) {
                                            const std::size_t operand_index =
                                                static_cast<std::size_t>(std::stoul(line.substr(index + 1, end - index - 1)));
                                            if (operand_index >= value.operands.size()) {
                                                throw std::runtime_error("inline asm operand index out of range");
                                            }
                                            rendered += operandText(value.operands[operand_index]);
                                            index = end;
                                            continue;
                                        }
                                    }
                                    rendered.push_back(line[index]);
                                }
                                return rendered;
                            };
                            for (const auto& line : value.lines) {
                                out << "    " << renderAsmLine(line) << '\n';
                            }
                        }
                    },
                    instruction.data);
            }
        }

        out << exit_label << ":\n";
        if (layout.frame_size > 0) {
            out << "    add rsp, " << layout.frame_size << '\n';
        }
        for (auto it = allocation.used_registers.rbegin(); it != allocation.used_registers.rend(); ++it) {
            out << "    pop " << registerName(*it) << '\n';
        }
        out << "    pop rbp\n";
        out << "    ret\n\n";
    }

    return out.str();
}

TargetPlatform defaultTarget() {
#ifdef _WIN32
    return TargetPlatform::Win64;
#else
    return TargetPlatform::SysV64;
#endif
}

std::string toString(const TargetPlatform target) {
    return target == TargetPlatform::Win64 ? "win64" : "sysv64";
}

std::optional<TargetPlatform> parseTarget(const std::string_view value) {
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "win64" || normalized == "windows" || normalized == "x64-windows") {
        return TargetPlatform::Win64;
    }
    if (normalized == "sysv64" || normalized == "linux" || normalized == "x64-sysv") {
        return TargetPlatform::SysV64;
    }
    return std::nullopt;
}

}  // namespace zenasm

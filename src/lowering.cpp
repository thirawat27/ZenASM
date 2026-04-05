#include "zenasm/lowering.hpp"

#include <functional>
#include <optional>
#include <unordered_map>

namespace zenasm {

namespace {

std::optional<std::int64_t> tryEvaluateIntConstant(const Expr& expr) {
    if (const auto* integer = dynamic_cast<const IntegerExpr*>(&expr)) {
        return integer->value;
    }
    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
        const auto operand = tryEvaluateIntConstant(*unary->operand);
        if (!operand.has_value()) {
            return std::nullopt;
        }
        if (unary->op == UnaryOp::Negate) {
            return -*operand;
        }
        return std::nullopt;
    }
    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
        const auto lhs = tryEvaluateIntConstant(*binary->lhs);
        const auto rhs = tryEvaluateIntConstant(*binary->rhs);
        if (!lhs.has_value() || !rhs.has_value()) {
            return std::nullopt;
        }
        switch (binary->op) {
            case BinaryOp::Add:
                return *lhs + *rhs;
            case BinaryOp::Sub:
                return *lhs - *rhs;
            case BinaryOp::Mul:
                return *lhs * *rhs;
            case BinaryOp::Div:
                if (*rhs == 0) {
                    return std::nullopt;
                }
                return *lhs / *rhs;
            case BinaryOp::Mod:
                if (*rhs == 0) {
                    return std::nullopt;
                }
                return *lhs % *rhs;
            default:
                return std::nullopt;
        }
    }
    return std::nullopt;
}

IROpcode lowerBinaryOpcode(const BinaryOp op) {
    switch (op) {
        case BinaryOp::Add:
            return IROpcode::Add;
        case BinaryOp::Sub:
            return IROpcode::Sub;
        case BinaryOp::Mul:
            return IROpcode::Mul;
        case BinaryOp::Div:
            return IROpcode::Div;
        case BinaryOp::Mod:
            return IROpcode::Mod;
        case BinaryOp::Equal:
            return IROpcode::CmpEq;
        case BinaryOp::NotEqual:
            return IROpcode::CmpNe;
        case BinaryOp::Less:
            return IROpcode::CmpLt;
        case BinaryOp::LessEqual:
            return IROpcode::CmpLe;
        case BinaryOp::Greater:
            return IROpcode::CmpGt;
        case BinaryOp::GreaterEqual:
            return IROpcode::CmpGe;
        case BinaryOp::LogicalAnd:
            return IROpcode::LogicalAnd;
        case BinaryOp::LogicalOr:
            return IROpcode::LogicalOr;
    }
    return IROpcode::Add;
}

IROpcode lowerUnaryOpcode(const UnaryOp op) {
    return op == UnaryOp::Negate ? IROpcode::Neg : IROpcode::Not;
}

bool containsLoopControl(const std::vector<std::unique_ptr<Stmt>>& statements) {
    for (const auto& statement : statements) {
        if (dynamic_cast<const BreakStmt*>(statement.get()) != nullptr || dynamic_cast<const ContinueStmt*>(statement.get()) != nullptr) {
            return true;
        }
        if (const auto* if_stmt = dynamic_cast<const IfStmt*>(statement.get())) {
            if (containsLoopControl(if_stmt->then_body) || containsLoopControl(if_stmt->else_body)) {
                return true;
            }
        } else if (const auto* while_stmt = dynamic_cast<const WhileStmt*>(statement.get())) {
            if (containsLoopControl(while_stmt->body)) {
                return true;
            }
        } else if (const auto* repeat_stmt = dynamic_cast<const RepeatStmt*>(statement.get())) {
            if (containsLoopControl(repeat_stmt->body)) {
                return true;
            }
        }
    }
    return false;
}

struct LoweringState {
    IRModule& module;
    std::unordered_map<std::string, std::string> string_pool;
    int next_string_id = 0;

    std::string internString(const std::string& value) {
        const auto found = string_pool.find(value);
        if (found != string_pool.end()) {
            return found->second;
        }
        const std::string label = "zenasm_str_" + std::to_string(next_string_id++);
        module.globals.push_back(IRModule::GlobalData {.label = label, .bytes = value});
        string_pool.emplace(value, label);
        return label;
    }
};

class FunctionLoweringContext {
  public:
    struct LoopControl {
        BlockId continue_target = -1;
        BlockId break_target = -1;
    };

    FunctionLoweringContext(const FunctionDecl& function, LoweringState& state, const SemanticModel& semantic, const LoweringOptions options)
        : function_(function),
          state_(state),
          semantic_(semantic),
          options_(options) {
        const auto signature_it = semantic_.functions.find(function.name);
        signature_ = signature_it->second;

        ir_.name = function.name;
        ir_.return_type = signature_.return_type;
        ir_.parameter_types = signature_.parameter_types;
        for (const auto& parameter : function.parameters) {
            ir_.parameter_names.push_back(parameter.name);
        }
        ir_.entry = createBlock("entry");
        current_block_ = ir_.entry;

        pushScope();
        for (std::size_t index = 0; index < function.parameters.size(); ++index) {
            const auto slot = allocateLocal(function.parameters[index].name, signature_.parameter_types[index]);
            scopes_.back().emplace(function.parameters[index].name, slot);
        }
    }

    IRFunction lower() {
        for (const auto& statement : function_.body) {
            lowerStatement(*statement);
        }
        if (!currentBlockTerminated()) {
            if (signature_.return_type == ValueType::Void) {
                emit(IRReturn {.value = std::nullopt}, function_.span());
            } else {
                const auto zero = emitConstant(0, function_.span());
                emit(IRReturn {.value = zero}, function_.span());
            }
        }
        popScope();
        return ir_;
    }

  private:
    [[nodiscard]] BlockId createBlock(const std::string& prefix) {
        const BlockId id = static_cast<BlockId>(ir_.blocks.size());
        ir_.blocks.push_back(IRBasicBlock {
            .id = id,
            .label = function_.name + "." + prefix + "." + std::to_string(id),
            .instructions = {},
        });
        return id;
    }

    [[nodiscard]] IRBasicBlock& block(const BlockId id) {
        return ir_.blocks[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] const IRBasicBlock& block(const BlockId id) const {
        return ir_.blocks[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] bool currentBlockTerminated() const {
        const auto& instructions = block(current_block_).instructions;
        return !instructions.empty() && isTerminator(instructions.back());
    }

    void emit(IRInstructionData data, const SourceSpan& span) {
        block(current_block_).instructions.push_back(IRInstruction {.data = std::move(data), .span = span});
    }

    [[nodiscard]] VirtualRegister nextVreg() {
        return ir_.next_vreg++;
    }

    [[nodiscard]] VirtualRegister emitConstant(const std::int64_t value, const SourceSpan& span) {
        const auto reg = nextVreg();
        emit(IRLoadImm {.dest = reg, .value = value}, span);
        return reg;
    }

    [[nodiscard]] LocalSlot allocateLocal(const std::string& name, const ValueType type) {
        const LocalSlot slot = static_cast<LocalSlot>(ir_.local_types.size());
        ir_.local_types.push_back(type);
        ir_.local_names.push_back(name);
        return slot;
    }

    void pushScope() {
        scopes_.emplace_back();
    }

    void popScope() {
        scopes_.pop_back();
    }

    [[nodiscard]] LocalSlot lookupLocal(const std::string& name, const SourceSpan& span) const {
        for (auto iter = scopes_.rbegin(); iter != scopes_.rend(); ++iter) {
            const auto found = iter->find(name);
            if (found != iter->end()) {
                return found->second;
            }
        }
        throw CompilationError("internal lowering error: missing local '" + name + "' at line " + std::to_string(span.begin.line));
    }

    [[nodiscard]] ValueType exprType(const Expr& expr) const {
        const auto found = semantic_.expr_types.find(&expr);
        if (found == semantic_.expr_types.end()) {
            return ValueType::Unknown;
        }
        return found->second;
    }

    [[nodiscard]] std::optional<LocalSlot> boundLocalForAsmOperand(const Expr& expr) const {
        if (const auto* identifier = dynamic_cast<const IdentifierExpr*>(&expr)) {
            return lookupLocal(identifier->name, identifier->span());
        }
        return std::nullopt;
    }

    [[nodiscard]] VirtualRegister lowerExpression(const Expr& expr) {
        if (const auto* integer = dynamic_cast<const IntegerExpr*>(&expr)) {
            return emitConstant(integer->value, expr.span());
        }

        if (const auto* boolean = dynamic_cast<const BoolExpr*>(&expr)) {
            return emitConstant(boolean->value ? 1 : 0, expr.span());
        }

        if (const auto* string = dynamic_cast<const StringExpr*>(&expr)) {
            const auto reg = nextVreg();
            emit(IRLoadAddress {.dest = reg, .symbol = state_.internString(string->value)}, expr.span());
            return reg;
        }

        if (const auto* identifier = dynamic_cast<const IdentifierExpr*>(&expr)) {
            const auto reg = nextVreg();
            emit(IRLoadLocal {.dest = reg, .local = lookupLocal(identifier->name, identifier->span())}, expr.span());
            return reg;
        }

        if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
            const auto operand = lowerExpression(*unary->operand);
            const auto dest = nextVreg();
            emit(IRUnary {.opcode = lowerUnaryOpcode(unary->op), .dest = dest, .value = operand}, expr.span());
            return dest;
        }

        if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
            const auto lhs = lowerExpression(*binary->lhs);
            const auto rhs = lowerExpression(*binary->rhs);
            const auto dest = nextVreg();
            emit(IRBinary {.opcode = lowerBinaryOpcode(binary->op), .dest = dest, .lhs = lhs, .rhs = rhs}, expr.span());
            return dest;
        }

        if (const auto* call = dynamic_cast<const CallExpr*>(&expr)) {
            std::vector<VirtualRegister> arguments;
            arguments.reserve(call->arguments.size());
            for (const auto& argument : call->arguments) {
                arguments.push_back(lowerExpression(*argument));
            }

            if (call->callee == "print") {
                ir_.contains_calls = true;
                emit(IRPrint {.value = arguments.at(0), .type = exprType(*call->arguments[0])}, expr.span());
                return emitConstant(0, expr.span());
            }

            ir_.contains_calls = true;
            const auto return_type = exprType(expr);
            if (return_type == ValueType::Void) {
                emit(IRCall {.dest = std::nullopt, .callee = call->callee, .arguments = std::move(arguments)}, expr.span());
                return emitConstant(0, expr.span());
            }
            const auto dest = nextVreg();
            emit(IRCall {.dest = dest, .callee = call->callee, .arguments = std::move(arguments)}, expr.span());
            return dest;
        }

        throw CompilationError("internal lowering error: unsupported expression");
    }

    void lowerScopedBlock(const std::vector<std::unique_ptr<Stmt>>& statements) {
        pushScope();
        for (const auto& statement : statements) {
            lowerStatement(*statement);
        }
        popScope();
    }

    void pushLoop(const BlockId continue_target, const BlockId break_target) {
        loop_stack_.push_back(LoopControl {.continue_target = continue_target, .break_target = break_target});
    }

    void popLoop() {
        loop_stack_.pop_back();
    }

    [[nodiscard]] const LoopControl& currentLoop(const SourceSpan& span) const {
        if (loop_stack_.empty()) {
            throw CompilationError("internal lowering error: loop control used outside loop at line " + std::to_string(span.begin.line));
        }
        return loop_stack_.back();
    }

    void lowerStatement(const Stmt& statement) {
        if (currentBlockTerminated()) {
            return;
        }

        if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
            const auto slot = allocateLocal(let->name, exprType(*let->initializer));
            scopes_.back().emplace(let->name, slot);
            const auto value = lowerExpression(*let->initializer);
            emit(IRStoreLocal {.local = slot, .value = value}, let->span());
            return;
        }

        if (const auto* assign = dynamic_cast<const AssignStmt*>(&statement)) {
            const auto value = lowerExpression(*assign->value);
            emit(IRStoreLocal {.local = lookupLocal(assign->name, assign->span()), .value = value}, assign->span());
            return;
        }

        if (const auto* ret = dynamic_cast<const ReturnStmt*>(&statement)) {
            if (ret->value == nullptr) {
                emit(IRReturn {.value = std::nullopt}, ret->span());
            } else {
                const auto value = lowerExpression(*ret->value);
                emit(IRReturn {.value = value}, ret->span());
            }
            return;
        }

        if (const auto* expr = dynamic_cast<const ExprStmt*>(&statement)) {
            static_cast<void>(lowerExpression(*expr->expr));
            return;
        }

        if (dynamic_cast<const BreakStmt*>(&statement) != nullptr) {
            emit(IRJump {.target = currentLoop(statement.span()).break_target}, statement.span());
            return;
        }

        if (dynamic_cast<const ContinueStmt*>(&statement) != nullptr) {
            emit(IRJump {.target = currentLoop(statement.span()).continue_target}, statement.span());
            return;
        }

        if (const auto* asm_stmt = dynamic_cast<const InlineAsmStmt*>(&statement)) {
            std::vector<IRInlineAsmOperand> operands;
            operands.reserve(asm_stmt->operands.size());
            for (const auto& operand : asm_stmt->operands) {
                IRInlineAsmOperand lowered_operand;
                lowered_operand.type = exprType(*operand);
                lowered_operand.local = boundLocalForAsmOperand(*operand);
                if (!lowered_operand.local.has_value()) {
                    lowered_operand.value = lowerExpression(*operand);
                }
                operands.push_back(std::move(lowered_operand));
            }
            emit(IRInlineAsm {.lines = asm_stmt->lines, .operands = std::move(operands)}, asm_stmt->span());
            return;
        }

        if (const auto* if_stmt = dynamic_cast<const IfStmt*>(&statement)) {
            const auto then_block = createBlock("if.then");
            const auto else_block = createBlock("if.else");
            const auto merge_block = createBlock("if.end");
            const auto condition = lowerExpression(*if_stmt->condition);
            emit(IRBranch {.condition = condition, .true_block = then_block, .false_block = else_block}, if_stmt->condition->span());

            current_block_ = then_block;
            lowerScopedBlock(if_stmt->then_body);
            if (!currentBlockTerminated()) {
                emit(IRJump {.target = merge_block}, if_stmt->span());
            }

            current_block_ = else_block;
            lowerScopedBlock(if_stmt->else_body);
            if (!currentBlockTerminated()) {
                emit(IRJump {.target = merge_block}, if_stmt->span());
            }

            current_block_ = merge_block;
            return;
        }

        if (const auto* while_stmt = dynamic_cast<const WhileStmt*>(&statement)) {
            const auto condition_block = createBlock("while.cond");
            const auto body_block = createBlock("while.body");
            const auto exit_block = createBlock("while.end");

            emit(IRJump {.target = condition_block}, while_stmt->span());

            current_block_ = condition_block;
            const auto condition = lowerExpression(*while_stmt->condition);
            emit(IRBranch {.condition = condition, .true_block = body_block, .false_block = exit_block}, while_stmt->condition->span());

            current_block_ = body_block;
            pushLoop(condition_block, exit_block);
            lowerScopedBlock(while_stmt->body);
            popLoop();
            if (!currentBlockTerminated()) {
                emit(IRJump {.target = condition_block}, while_stmt->span());
            }

            current_block_ = exit_block;
            return;
        }

        if (const auto* for_stmt = dynamic_cast<const ForStmt*>(&statement)) {
            pushScope();
            const auto iterator_slot = allocateLocal(for_stmt->variable_name, ValueType::Int64);
            scopes_.back().emplace(for_stmt->variable_name, iterator_slot);
            const auto end_slot = allocateLocal("__for_end." + std::to_string(temp_index_), ValueType::Int64);
            const auto step_slot = allocateLocal("__for_step." + std::to_string(temp_index_), ValueType::Int64);
            ++temp_index_;

            emit(IRStoreLocal {.local = iterator_slot, .value = lowerExpression(*for_stmt->start)}, for_stmt->start->span());
            emit(IRStoreLocal {.local = end_slot, .value = lowerExpression(*for_stmt->end)}, for_stmt->end->span());
            emit(IRStoreLocal {.local = step_slot, .value = lowerExpression(*for_stmt->step)}, for_stmt->step->span());

            const auto dispatch_block = createBlock("for.dispatch");
            const auto positive_check_block = createBlock("for.cond.pos");
            const auto negative_check_block = createBlock("for.cond.neg");
            const auto body_block = createBlock("for.body");
            const auto increment_block = createBlock("for.inc");
            const auto exit_block = createBlock("for.end");

            emit(IRJump {.target = dispatch_block}, for_stmt->span());

            current_block_ = dispatch_block;
            const auto step_value = nextVreg();
            emit(IRLoadLocal {.dest = step_value, .local = step_slot}, for_stmt->span());
            const auto zero = emitConstant(0, for_stmt->span());
            const auto step_positive = nextVreg();
            emit(IRBinary {.opcode = IROpcode::CmpGt, .dest = step_positive, .lhs = step_value, .rhs = zero}, for_stmt->span());
            emit(IRBranch {.condition = step_positive, .true_block = positive_check_block, .false_block = negative_check_block}, for_stmt->span());

            current_block_ = positive_check_block;
            const auto positive_iter = nextVreg();
            emit(IRLoadLocal {.dest = positive_iter, .local = iterator_slot}, for_stmt->span());
            const auto positive_end = nextVreg();
            emit(IRLoadLocal {.dest = positive_end, .local = end_slot}, for_stmt->span());
            const auto positive_cmp = nextVreg();
            emit(IRBinary {.opcode = IROpcode::CmpLt, .dest = positive_cmp, .lhs = positive_iter, .rhs = positive_end}, for_stmt->span());
            emit(IRBranch {.condition = positive_cmp, .true_block = body_block, .false_block = exit_block}, for_stmt->span());

            current_block_ = negative_check_block;
            const auto negative_iter = nextVreg();
            emit(IRLoadLocal {.dest = negative_iter, .local = iterator_slot}, for_stmt->span());
            const auto negative_end = nextVreg();
            emit(IRLoadLocal {.dest = negative_end, .local = end_slot}, for_stmt->span());
            const auto negative_cmp = nextVreg();
            emit(IRBinary {.opcode = IROpcode::CmpGt, .dest = negative_cmp, .lhs = negative_iter, .rhs = negative_end}, for_stmt->span());
            emit(IRBranch {.condition = negative_cmp, .true_block = body_block, .false_block = exit_block}, for_stmt->span());

            current_block_ = body_block;
            pushLoop(increment_block, exit_block);
            lowerScopedBlock(for_stmt->body);
            popLoop();
            if (!currentBlockTerminated()) {
                emit(IRJump {.target = increment_block}, for_stmt->span());
            }

            current_block_ = increment_block;
            const auto loop_value = nextVreg();
            emit(IRLoadLocal {.dest = loop_value, .local = iterator_slot}, for_stmt->span());
            const auto loop_step = nextVreg();
            emit(IRLoadLocal {.dest = loop_step, .local = step_slot}, for_stmt->span());
            const auto next_value = nextVreg();
            emit(IRBinary {.opcode = IROpcode::Add, .dest = next_value, .lhs = loop_value, .rhs = loop_step}, for_stmt->span());
            emit(IRStoreLocal {.local = iterator_slot, .value = next_value}, for_stmt->span());
            emit(IRJump {.target = dispatch_block}, for_stmt->span());

            current_block_ = exit_block;
            popScope();
            return;
        }

        if (const auto* repeat_stmt = dynamic_cast<const RepeatStmt*>(&statement)) {
            if (options_.opt_level >= 2 && !containsLoopControl(repeat_stmt->body)) {
                const auto repeat_count = tryEvaluateIntConstant(*repeat_stmt->count);
                if (repeat_count.has_value() && *repeat_count >= 0 && *repeat_count <= 8) {
                    for (std::int64_t iteration = 0; iteration < *repeat_count; ++iteration) {
                        lowerScopedBlock(repeat_stmt->body);
                    }
                    return;
                }
            }

            const auto limit_slot = allocateLocal("__repeat_limit." + std::to_string(temp_index_), ValueType::Int64);
            const auto counter_slot = allocateLocal("__repeat_counter." + std::to_string(temp_index_), ValueType::Int64);
            ++temp_index_;

            const auto count_value = lowerExpression(*repeat_stmt->count);
            emit(IRStoreLocal {.local = limit_slot, .value = count_value}, repeat_stmt->count->span());
            emit(IRStoreLocal {.local = counter_slot, .value = emitConstant(0, repeat_stmt->count->span())}, repeat_stmt->count->span());

            const auto condition_block = createBlock("repeat.cond");
            const auto body_block = createBlock("repeat.body");
            const auto increment_block = createBlock("repeat.inc");
            const auto exit_block = createBlock("repeat.end");

            emit(IRJump {.target = condition_block}, repeat_stmt->span());

            current_block_ = condition_block;
            const auto counter_reg = nextVreg();
            emit(IRLoadLocal {.dest = counter_reg, .local = counter_slot}, repeat_stmt->span());
            const auto limit_reg = nextVreg();
            emit(IRLoadLocal {.dest = limit_reg, .local = limit_slot}, repeat_stmt->span());
            const auto cmp_reg = nextVreg();
            emit(IRBinary {.opcode = IROpcode::CmpLt, .dest = cmp_reg, .lhs = counter_reg, .rhs = limit_reg}, repeat_stmt->span());
            emit(IRBranch {.condition = cmp_reg, .true_block = body_block, .false_block = exit_block}, repeat_stmt->span());

            current_block_ = body_block;
            pushLoop(increment_block, exit_block);
            lowerScopedBlock(repeat_stmt->body);
            popLoop();
            if (!currentBlockTerminated()) {
                emit(IRJump {.target = increment_block}, repeat_stmt->span());
            }

            current_block_ = increment_block;
            const auto loaded_counter = nextVreg();
            emit(IRLoadLocal {.dest = loaded_counter, .local = counter_slot}, repeat_stmt->span());
            const auto one = emitConstant(1, repeat_stmt->span());
            const auto incremented = nextVreg();
            emit(IRBinary {.opcode = IROpcode::Add, .dest = incremented, .lhs = loaded_counter, .rhs = one}, repeat_stmt->span());
            emit(IRStoreLocal {.local = counter_slot, .value = incremented}, repeat_stmt->span());
            emit(IRJump {.target = condition_block}, repeat_stmt->span());

            current_block_ = exit_block;
            return;
        }

        throw CompilationError("internal lowering error: unsupported statement");
    }

    const FunctionDecl& function_;
    LoweringState& state_;
    const SemanticModel& semantic_;
    LoweringOptions options_;
    FunctionSignature signature_ {};
    IRFunction ir_ {};
    BlockId current_block_ = -1;
    int temp_index_ = 0;
    std::vector<LoopControl> loop_stack_ {};
    std::vector<std::unordered_map<std::string, LocalSlot>> scopes_;
};

}  // namespace

Lowerer::Lowerer(const SemanticModel& semantic, const LoweringOptions options)
    : semantic_(semantic),
      options_(options) {}

IRModule Lowerer::lower(const Program& program) {
    IRModule module;
    LoweringState state {.module = module, .string_pool = {}, .next_string_id = 0};
    for (const auto& decl : program.declarations) {
        if (const auto* external = dynamic_cast<const ExternFunctionDecl*>(decl.get())) {
            const auto signature = semantic_.functions.at(external->name);
            std::vector<std::string> parameter_names;
            for (const auto& parameter : external->parameters) {
                parameter_names.push_back(parameter.name);
            }
            module.functions.push_back(IRFunction {
                .name = external->name,
                .is_extern = true,
                .contains_calls = false,
                .return_type = signature.return_type,
                .parameter_types = signature.parameter_types,
                .parameter_names = std::move(parameter_names),
                .local_types = {},
                .local_names = {},
                .blocks = {},
                .entry = -1,
                .next_vreg = 0,
            });
        }
    }

    for (const auto& decl : program.declarations) {
        if (const auto* function = dynamic_cast<const FunctionDecl*>(decl.get())) {
            FunctionLoweringContext context(*function, state, semantic_, options_);
            module.functions.push_back(context.lower());
        }
    }
    return module;
}

}  // namespace zenasm

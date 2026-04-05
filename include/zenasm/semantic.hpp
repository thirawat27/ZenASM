#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "zenasm/ast.hpp"
#include "zenasm/diagnostics.hpp"

namespace zenasm {

struct FunctionSignature {
    std::string name;
    bool is_extern = false;
    std::vector<ValueType> parameter_types;
    ValueType return_type = ValueType::Unknown;
    bool explicit_return_type = false;
};

struct SemanticModel {
    std::unordered_map<const Expr*, ValueType> expr_types;
    std::unordered_map<std::string, FunctionSignature> functions;
};

class SemanticAnalyzer {
  public:
    SemanticAnalyzer(Diagnostics& diagnostics, SemanticModel& model);

    void analyze(const Program& program);

  private:
    void declareFunctions(const Program& program);
    void analyzeFunction(const FunctionDecl& function);
    void analyzeStatement(const Stmt& statement);
    void analyzeBlock(const std::vector<std::unique_ptr<Stmt>>& statements);
    [[nodiscard]] ValueType analyzeExpression(const Expr& expr);
    void pushScope();
    void popScope();
    void declareVariable(const std::string& name, ValueType type, const SourceSpan& span);
    [[nodiscard]] ValueType lookupVariable(const std::string& name, const SourceSpan& span) const;
    [[nodiscard]] FunctionSignature* lookupFunction(const std::string& name);
    [[nodiscard]] const FunctionSignature* lookupFunction(const std::string& name) const;
    [[nodiscard]] bool canAssign(ValueType target, ValueType source) const noexcept;
    [[nodiscard]] ValueType mergeReturnType(ValueType current, ValueType candidate, const SourceSpan& span);
    [[noreturn]] void fail(const SourceSpan& span, const std::string& message) const;

    Diagnostics& diagnostics_;
    SemanticModel& model_;
    std::vector<std::unordered_map<std::string, ValueType>> scopes_ {};
    FunctionSignature* current_function_ = nullptr;
    bool current_function_has_return_ = false;
    int loop_depth_ = 0;
};

}  // namespace zenasm

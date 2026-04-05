#pragma once

#include "zenasm/ast.hpp"
#include "zenasm/ir.hpp"
#include "zenasm/semantic.hpp"

namespace zenasm {

struct LoweringOptions {
    int opt_level = 2;
};

class Lowerer {
  public:
    Lowerer(const SemanticModel& semantic, LoweringOptions options);

    [[nodiscard]] IRModule lower(const Program& program);

    const SemanticModel& semantic_;
    LoweringOptions options_;
};

}  // namespace zenasm

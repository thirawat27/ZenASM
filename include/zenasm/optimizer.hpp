#pragma once

#include <string>

#include "zenasm/ir.hpp"

namespace zenasm {

class Optimizer {
  public:
    explicit Optimizer(int opt_level);

    void run(IRModule& module) const;

  private:
    void optimizeFunction(IRFunction& function) const;
    void foldConstants(IRFunction& function) const;
    void simplifyBranches(IRFunction& function) const;
    void eliminateDeadStores(IRFunction& function) const;
    void eliminateDeadCode(IRFunction& function) const;
    void removeUnreachableBlocks(IRFunction& function) const;

    int opt_level_ = 0;
};

[[nodiscard]] std::string dumpIR(const IRModule& module);

}  // namespace zenasm

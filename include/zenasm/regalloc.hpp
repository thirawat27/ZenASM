#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "zenasm/ir.hpp"

namespace zenasm {

enum class PhysReg {
    Rbx,
    R12,
    R13,
    R14,
    R15,
};

struct AllocatedLocation {
    bool spilled = false;
    PhysReg reg = PhysReg::Rbx;
    int spill_slot = -1;
};

struct FunctionAllocation {
    std::unordered_map<VirtualRegister, AllocatedLocation> locations;
    std::vector<PhysReg> used_registers;
    int spill_count = 0;
};

class RegisterAllocator {
  public:
    [[nodiscard]] FunctionAllocation allocate(const IRFunction& function) const;
};

[[nodiscard]] std::string registerName(PhysReg reg);

}  // namespace zenasm

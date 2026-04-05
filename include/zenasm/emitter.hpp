#pragma once

#include <optional>
#include <string>

#include "zenasm/ir.hpp"
#include "zenasm/regalloc.hpp"
#include "zenasm/source.hpp"

namespace zenasm {

enum class TargetPlatform {
    Win64,
    SysV64,
};

struct EmitOptions {
    TargetPlatform target = TargetPlatform::Win64;
    bool annotate_source = true;
};

class Emitter {
  public:
    Emitter(const SourceFile& source, EmitOptions options);

    [[nodiscard]] std::string emit(const IRModule& module) const;

  private:
    const SourceFile& source_;
    EmitOptions options_;
};

[[nodiscard]] TargetPlatform defaultTarget();
[[nodiscard]] std::string toString(TargetPlatform target);
[[nodiscard]] std::optional<TargetPlatform> parseTarget(std::string_view value);

}  // namespace zenasm

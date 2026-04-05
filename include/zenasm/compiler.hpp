#pragma once

#include <string>

#include "zenasm/emitter.hpp"

namespace zenasm {

struct BuildOptions {
    std::string input_path;
    std::string output_path;
    int opt_level = 3;
    TargetPlatform target = TargetPlatform::Win64;
    bool annotate_source = true;
    std::string ir_output_path;
    std::string ast_output_path;
};

struct BuildResult {
    std::string assembly;
    std::string diagnostics;
    std::string ir_dump;
    std::string ast_dump;
};

class Compiler {
  public:
    [[nodiscard]] BuildResult build(const BuildOptions& options) const;
};

}  // namespace zenasm

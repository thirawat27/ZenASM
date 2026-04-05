#include "zenasm/regalloc.hpp"

#include <algorithm>
#include <set>
#include <unordered_set>

namespace zenasm {

namespace {

constexpr PhysReg kRegisters[] = {
    PhysReg::Rbx,
    PhysReg::R12,
    PhysReg::R13,
    PhysReg::R14,
    PhysReg::R15,
};

using InterferenceGraph = std::unordered_map<VirtualRegister, std::unordered_set<VirtualRegister>>;

void addEdge(InterferenceGraph& graph, const VirtualRegister lhs, const VirtualRegister rhs) {
    if (lhs == rhs) {
        return;
    }
    graph[lhs].insert(rhs);
    graph[rhs].insert(lhs);
}

}  // namespace

FunctionAllocation RegisterAllocator::allocate(const IRFunction& function) const {
    FunctionAllocation allocation;
    if (function.is_extern) {
        return allocation;
    }

    std::unordered_map<BlockId, std::unordered_set<VirtualRegister>> use_sets;
    std::unordered_map<BlockId, std::unordered_set<VirtualRegister>> def_sets;
    std::unordered_map<BlockId, std::unordered_set<VirtualRegister>> live_in;
    std::unordered_map<BlockId, std::unordered_set<VirtualRegister>> live_out;
    InterferenceGraph graph;

    for (const auto& block : function.blocks) {
        auto& use = use_sets[block.id];
        auto& def = def_sets[block.id];
        for (const auto& instruction : block.instructions) {
            for (const auto reg : usesOf(instruction)) {
                if (!def.contains(reg)) {
                    use.insert(reg);
                }
                graph.try_emplace(reg);
            }
            if (const auto defined = defOf(instruction); defined.has_value()) {
                def.insert(*defined);
                graph.try_emplace(*defined);
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto block_it = function.blocks.rbegin(); block_it != function.blocks.rend(); ++block_it) {
            const auto& block = *block_it;
            auto new_out = std::unordered_set<VirtualRegister> {};
            for (const auto successor : successorsOf(block)) {
                const auto& successor_live_in = live_in[successor];
                new_out.insert(successor_live_in.begin(), successor_live_in.end());
            }

            auto new_in = use_sets[block.id];
            for (const auto reg : new_out) {
                if (!def_sets[block.id].contains(reg)) {
                    new_in.insert(reg);
                }
            }

            if (new_out != live_out[block.id] || new_in != live_in[block.id]) {
                live_out[block.id] = std::move(new_out);
                live_in[block.id] = std::move(new_in);
                changed = true;
            }
        }
    }

    for (const auto& block : function.blocks) {
        auto live = live_out[block.id];
        for (auto instruction_it = block.instructions.rbegin(); instruction_it != block.instructions.rend(); ++instruction_it) {
            const auto defined = defOf(*instruction_it);
            if (defined.has_value()) {
                graph.try_emplace(*defined);
                for (const auto reg : live) {
                    addEdge(graph, *defined, reg);
                }
                live.erase(*defined);
            }
            for (const auto reg : usesOf(*instruction_it)) {
                graph.try_emplace(reg);
                live.insert(reg);
            }
        }
    }

    InterferenceGraph mutable_graph = graph;
    struct NodeSelection {
        VirtualRegister reg = -1;
        bool potential_spill = false;
    };
    std::vector<NodeSelection> stack;
    stack.reserve(graph.size());

    while (!mutable_graph.empty()) {
        auto selected = mutable_graph.end();
        for (auto it = mutable_graph.begin(); it != mutable_graph.end(); ++it) {
            if (it->second.size() < std::size(kRegisters)) {
                selected = it;
                break;
            }
        }

        bool spill = false;
        if (selected == mutable_graph.end()) {
            spill = true;
            selected = std::max_element(
                mutable_graph.begin(),
                mutable_graph.end(),
                [](const auto& lhs, const auto& rhs) { return lhs.second.size() < rhs.second.size(); });
        }

        const auto reg = selected->first;
        for (const auto neighbor : selected->second) {
            mutable_graph[neighbor].erase(reg);
        }
        mutable_graph.erase(selected);
        stack.push_back(NodeSelection {.reg = reg, .potential_spill = spill});
    }

    std::unordered_map<VirtualRegister, int> colors;
    int next_spill = 0;
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        std::set<int> unavailable;
        for (const auto neighbor : graph[it->reg]) {
            const auto colored = colors.find(neighbor);
            if (colored != colors.end()) {
                unavailable.insert(colored->second);
            }
        }

        int color = -1;
        for (int index = 0; index < static_cast<int>(std::size(kRegisters)); ++index) {
            if (!unavailable.contains(index)) {
                color = index;
                break;
            }
        }

        if (color < 0) {
            allocation.locations[it->reg] = AllocatedLocation {.spilled = true, .reg = PhysReg::Rbx, .spill_slot = next_spill++};
            continue;
        }

        colors[it->reg] = color;
        allocation.locations[it->reg] = AllocatedLocation {.spilled = false, .reg = kRegisters[color], .spill_slot = -1};
    }

    allocation.spill_count = next_spill;
    std::unordered_set<PhysReg> used;
    for (const auto& [reg, location] : allocation.locations) {
        static_cast<void>(reg);
        if (!location.spilled && used.insert(location.reg).second) {
            allocation.used_registers.push_back(location.reg);
        }
    }
    std::sort(
        allocation.used_registers.begin(),
        allocation.used_registers.end(),
        [](const PhysReg lhs, const PhysReg rhs) { return static_cast<int>(lhs) < static_cast<int>(rhs); });
    return allocation;
}

std::string registerName(const PhysReg reg) {
    switch (reg) {
        case PhysReg::Rbx:
            return "rbx";
        case PhysReg::R12:
            return "r12";
        case PhysReg::R13:
            return "r13";
        case PhysReg::R14:
            return "r14";
        case PhysReg::R15:
            return "r15";
    }
    return "rbx";
}

}  // namespace zenasm

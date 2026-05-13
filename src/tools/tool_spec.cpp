#include "tools/tool_spec.hpp"

#include "state.hpp"
#include "tools/throw_bomb.hpp"
#include "tools/throw_pot.hpp"
#include "tools/throw_rope.hpp"

#include <array>
#include <cassert>

namespace splonks {

namespace {

std::array<ToolSpec, kToolKindCount> g_tool_specs{};
bool g_tool_specs_populated = false;

void SetSpec(ToolKind kind, const ToolSpec& spec) {
    g_tool_specs[ToolKindIndex(kind)] = spec;
}

} // namespace

const ToolSpec& GetToolSpec(ToolKind kind) {
    assert(g_tool_specs_populated && "PopulateToolSpecsTable must run before lookup");
    return g_tool_specs[ToolKindIndex(kind)];
}

const char* GetToolKindName(ToolKind kind) {
    if (!g_tool_specs_populated) {
        return "Unknown";
    }

    const char* const debug_name = g_tool_specs[ToolKindIndex(kind)].debug_name;
    return debug_name != nullptr ? debug_name : "Unknown";
}

std::optional<ToolKind> FindPreferredToolKindForSlotIndex(std::size_t slot_index) {
    for (std::size_t tool_index = 0; tool_index < kToolKindCount; ++tool_index) {
        const ToolKind kind = static_cast<ToolKind>(tool_index);
        const ToolSpec& spec = GetToolSpec(kind);
        if (spec.preferred_slot_index.has_value() &&
            *spec.preferred_slot_index == slot_index) {
            return kind;
        }
    }
    return std::nullopt;
}

void PopulateToolSpecsTable() {
    SetSpec(ToolKind::ThrowPot, tools::throw_pot::kThrowPotToolSpec);
    SetSpec(ToolKind::ThrowBomb, tools::throw_bomb::kThrowBombToolSpec);
    SetSpec(ToolKind::ThrowRope, tools::throw_rope::kThrowRopeToolSpec);
    SetSpec(ToolKind::ThrowStickyBomb, tools::throw_bomb::kThrowStickyBombToolSpec);
    g_tool_specs_populated = true;
}

void FillToolSlot(ToolSlot& slot, ToolKind kind, std::uint16_t count, bool active) {
    const ToolSpec& tool_spec = GetToolSpec(kind);
    slot.kind = tool_spec.kind;
    slot.count = count;
    slot.cooldown = 0;
    slot.active = active;
}

} // namespace splonks

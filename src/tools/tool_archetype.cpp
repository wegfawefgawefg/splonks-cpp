#include "tools/tool_archetype.hpp"

#include "state.hpp"
#include "tools/throw_bomb.hpp"
#include "tools/throw_pot.hpp"
#include "tools/throw_rope.hpp"

#include <array>
#include <cassert>

namespace splonks {

namespace {

std::array<ToolArchetype, kToolKindCount> g_tool_archetypes{};
bool g_tool_archetypes_populated = false;

void SetArchetype(ToolKind kind, const ToolArchetype& archetype) {
    g_tool_archetypes[ToolKindIndex(kind)] = archetype;
}

} // namespace

const ToolArchetype& GetToolArchetype(ToolKind kind) {
    assert(g_tool_archetypes_populated && "PopulateToolArchetypesTable must run before lookup");
    return g_tool_archetypes[ToolKindIndex(kind)];
}

const char* GetToolKindName(ToolKind kind) {
    if (!g_tool_archetypes_populated) {
        return "Unknown";
    }

    const char* const debug_name = g_tool_archetypes[ToolKindIndex(kind)].debug_name;
    return debug_name != nullptr ? debug_name : "Unknown";
}

std::optional<ToolKind> FindPreferredToolKindForSlotIndex(std::size_t slot_index) {
    for (std::size_t tool_index = 0; tool_index < kToolKindCount; ++tool_index) {
        const ToolKind kind = static_cast<ToolKind>(tool_index);
        const ToolArchetype& archetype = GetToolArchetype(kind);
        if (archetype.preferred_slot_index.has_value() &&
            *archetype.preferred_slot_index == slot_index) {
            return kind;
        }
    }
    return std::nullopt;
}

void PopulateToolArchetypesTable() {
    SetArchetype(ToolKind::ThrowPot, tools::throw_pot::kThrowPotToolArchetype);
    SetArchetype(ToolKind::ThrowBomb, tools::throw_bomb::kThrowBombToolArchetype);
    SetArchetype(ToolKind::ThrowRope, tools::throw_rope::kThrowRopeToolArchetype);
    g_tool_archetypes_populated = true;
}

void FillToolSlot(ToolSlot& slot, ToolKind kind, std::uint16_t count, bool active) {
    const ToolArchetype& tool_archetype = GetToolArchetype(kind);
    slot.kind = tool_archetype.kind;
    slot.count = count;
    slot.cooldown = 0;
    slot.active = active;
}

} // namespace splonks

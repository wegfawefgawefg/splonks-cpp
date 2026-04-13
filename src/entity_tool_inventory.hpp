#pragma once

#include "sid.hpp"
#include "tools/tool_archetype.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace splonks {

constexpr std::size_t kToolSlotCount = 2;

struct ToolSlot {
    ToolKind kind = ToolKind::ThrowPot;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    bool active = false;
};

struct EntityToolState {
    VID owner_vid;
    std::array<ToolSlot, kToolSlotCount> slots{};
};

struct EntityToolInventoryState {
    std::vector<EntityToolState> tool_states;

    void Step();
    EntityToolState* FindEntityToolStateMut(const VID& owner_vid);
    const EntityToolState* FindEntityToolState(const VID& owner_vid) const;
    ToolSlot* FindToolSlotMut(const VID& owner_vid, std::size_t slot_index);
    const ToolSlot* FindToolSlot(const VID& owner_vid, std::size_t slot_index) const;
    ToolSlot& EnsureToolSlot(const VID& owner_vid, std::size_t slot_index);
};

} // namespace splonks

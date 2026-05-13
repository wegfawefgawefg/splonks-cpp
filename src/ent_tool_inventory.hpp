#pragma once

#include "sid.hpp"
#include "tools/tool_spec.hpp"

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

struct EntToolState {
    VID owner_vid;
    std::array<ToolSlot, kToolSlotCount> slots{};
};

struct EntToolInventoryState {
    std::vector<EntToolState> tool_states;

    void Step();
    EntToolState* FindEntToolStateMut(const VID& owner_vid);
    const EntToolState* FindEntToolState(const VID& owner_vid) const;
    ToolSlot* FindToolSlotMut(const VID& owner_vid, std::size_t slot_index);
    const ToolSlot* FindToolSlot(const VID& owner_vid, std::size_t slot_index) const;
    ToolSlot& EnsureToolSlot(const VID& owner_vid, std::size_t slot_index);
    bool AddToolCount(const VID& owner_vid, ToolKind kind, std::uint32_t amount);
    bool UpgradeBombsToSticky(const VID& owner_vid);
};

} // namespace splonks

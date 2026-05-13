#include "ent_tool_inventory.hpp"

#include <algorithm>

namespace splonks {

void EntToolInventoryState::Step() {
    for (EntToolState& tool_state : tool_states) {
        for (ToolSlot& slot : tool_state.slots) {
            if (!slot.active) {
                continue;
            }
            if (slot.cooldown > 0) {
                slot.cooldown -= 1;
            }
        }
    }
}

EntToolState* EntToolInventoryState::FindEntToolStateMut(const VID& owner_vid) {
    for (EntToolState& tool_state : tool_states) {
        if (tool_state.owner_vid == owner_vid) {
            return &tool_state;
        }
    }
    return nullptr;
}

const EntToolState* EntToolInventoryState::FindEntToolState(const VID& owner_vid) const {
    for (const EntToolState& tool_state : tool_states) {
        if (tool_state.owner_vid == owner_vid) {
            return &tool_state;
        }
    }
    return nullptr;
}

ToolSlot* EntToolInventoryState::FindToolSlotMut(const VID& owner_vid, std::size_t slot_index) {
    EntToolState* const tool_state = FindEntToolStateMut(owner_vid);
    if (tool_state == nullptr || slot_index >= tool_state->slots.size()) {
        return nullptr;
    }
    return &tool_state->slots[slot_index];
}

const ToolSlot* EntToolInventoryState::FindToolSlot(const VID& owner_vid, std::size_t slot_index) const {
    const EntToolState* const tool_state = FindEntToolState(owner_vid);
    if (tool_state == nullptr || slot_index >= tool_state->slots.size()) {
        return nullptr;
    }
    return &tool_state->slots[slot_index];
}

ToolSlot& EntToolInventoryState::EnsureToolSlot(const VID& owner_vid, std::size_t slot_index) {
    if (EntToolState* existing = FindEntToolStateMut(owner_vid)) {
        return existing->slots[slot_index];
    }
    EntToolState tool_state{};
    tool_state.owner_vid = owner_vid;
    tool_states.push_back(tool_state);
    return tool_states.back().slots[slot_index];
}

bool EntToolInventoryState::AddToolCount(
    const VID& owner_vid,
    ToolKind kind,
    std::uint32_t amount
) {
    if (amount == 0) {
        return false;
    }

    EntToolState* tool_state = FindEntToolStateMut(owner_vid);
    if (tool_state == nullptr) {
        EntToolState new_tool_state{};
        new_tool_state.owner_vid = owner_vid;
        tool_states.push_back(new_tool_state);
        tool_state = &tool_states.back();
    }

    ToolSlot* slot = nullptr;
    if (kind == ToolKind::ThrowBomb) {
        for (ToolSlot& candidate : tool_state->slots) {
            if (candidate.active && candidate.kind == ToolKind::ThrowStickyBomb) {
                slot = &candidate;
                break;
            }
        }
    }

    for (ToolSlot& candidate : tool_state->slots) {
        if (slot != nullptr) {
            break;
        }
        if (candidate.active && candidate.kind == kind) {
            slot = &candidate;
            break;
        }
    }

    if (slot == nullptr) {
        const std::optional<std::size_t> preferred_slot_index = GetToolSpec(kind).preferred_slot_index;
        if (preferred_slot_index.has_value() && *preferred_slot_index < tool_state->slots.size()) {
            ToolSlot& preferred_slot = tool_state->slots[*preferred_slot_index];
            if (!preferred_slot.active) {
                slot = &preferred_slot;
            }
        }
    }

    if (slot == nullptr) {
        for (ToolSlot& candidate : tool_state->slots) {
            if (!candidate.active) {
                slot = &candidate;
                break;
            }
        }
    }

    if (slot == nullptr) {
        return false;
    }

    if (!slot->active) {
        FillToolSlot(*slot, kind, 0, true);
    }

    constexpr std::uint32_t kMaxToolSlotCount = UINT16_MAX;
    const std::uint32_t new_count = std::min<std::uint32_t>(
        kMaxToolSlotCount,
        static_cast<std::uint32_t>(slot->count) + amount
    );
    slot->count = static_cast<std::uint16_t>(new_count);
    return true;
}

bool EntToolInventoryState::UpgradeBombsToSticky(const VID& owner_vid) {
    EntToolState* tool_state = FindEntToolStateMut(owner_vid);
    if (tool_state == nullptr) {
        EntToolState new_tool_state{};
        new_tool_state.owner_vid = owner_vid;
        tool_states.push_back(new_tool_state);
        tool_state = &tool_states.back();
    }

    for (ToolSlot& candidate : tool_state->slots) {
        if (candidate.active && candidate.kind == ToolKind::ThrowStickyBomb) {
            return true;
        }
    }

    for (ToolSlot& candidate : tool_state->slots) {
        if (candidate.active && candidate.kind == ToolKind::ThrowBomb) {
            candidate.kind = ToolKind::ThrowStickyBomb;
            return true;
        }
    }

    const std::optional<std::size_t> preferred_slot_index =
        GetToolSpec(ToolKind::ThrowStickyBomb).preferred_slot_index;
    if (preferred_slot_index.has_value() && *preferred_slot_index < tool_state->slots.size()) {
        ToolSlot& preferred_slot = tool_state->slots[*preferred_slot_index];
        if (!preferred_slot.active) {
            FillToolSlot(preferred_slot, ToolKind::ThrowStickyBomb, 0, true);
            return true;
        }
    }

    for (ToolSlot& candidate : tool_state->slots) {
        if (!candidate.active) {
            FillToolSlot(candidate, ToolKind::ThrowStickyBomb, 0, true);
            return true;
        }
    }

    return false;
}

} // namespace splonks

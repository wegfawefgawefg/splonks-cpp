#include "entity_tool_inventory.hpp"

namespace splonks {

void EntityToolInventoryState::Step() {
    for (EntityToolState& tool_state : tool_states) {
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

EntityToolState* EntityToolInventoryState::FindEntityToolStateMut(const VID& owner_vid) {
    for (EntityToolState& tool_state : tool_states) {
        if (tool_state.owner_vid == owner_vid) {
            return &tool_state;
        }
    }
    return nullptr;
}

const EntityToolState* EntityToolInventoryState::FindEntityToolState(const VID& owner_vid) const {
    for (const EntityToolState& tool_state : tool_states) {
        if (tool_state.owner_vid == owner_vid) {
            return &tool_state;
        }
    }
    return nullptr;
}

ToolSlot* EntityToolInventoryState::FindToolSlotMut(const VID& owner_vid, std::size_t slot_index) {
    EntityToolState* const tool_state = FindEntityToolStateMut(owner_vid);
    if (tool_state == nullptr || slot_index >= tool_state->slots.size()) {
        return nullptr;
    }
    return &tool_state->slots[slot_index];
}

const ToolSlot* EntityToolInventoryState::FindToolSlot(const VID& owner_vid, std::size_t slot_index) const {
    const EntityToolState* const tool_state = FindEntityToolState(owner_vid);
    if (tool_state == nullptr || slot_index >= tool_state->slots.size()) {
        return nullptr;
    }
    return &tool_state->slots[slot_index];
}

ToolSlot& EntityToolInventoryState::EnsureToolSlot(const VID& owner_vid, std::size_t slot_index) {
    if (EntityToolState* existing = FindEntityToolStateMut(owner_vid)) {
        return existing->slots[slot_index];
    }
    EntityToolState tool_state{};
    tool_state.owner_vid = owner_vid;
    tool_states.push_back(tool_state);
    return tool_states.back().slots[slot_index];
}

} // namespace splonks

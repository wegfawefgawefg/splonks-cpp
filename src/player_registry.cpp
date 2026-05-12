#include "player_registry.hpp"

#include <algorithm>

namespace splonks {

PlayerRegistry PlayerRegistry::New() {
    PlayerRegistry registry;
    (void)registry.EnsurePrimaryLocalPlayer();
    return registry;
}

PlayerSlot* PlayerRegistry::Find(PlayerId player_id) {
    for (PlayerSlot& slot : slots) {
        if (slot.player_id == player_id) {
            return &slot;
        }
    }
    return nullptr;
}

const PlayerSlot* PlayerRegistry::Find(PlayerId player_id) const {
    for (const PlayerSlot& slot : slots) {
        if (slot.player_id == player_id) {
            return &slot;
        }
    }
    return nullptr;
}

PlayerSlot* PlayerRegistry::FindByEntityVid(VID entity_vid) {
    for (PlayerSlot& slot : slots) {
        if (slot.entity_vid.has_value() && *slot.entity_vid == entity_vid) {
            return &slot;
        }
    }
    return nullptr;
}

const PlayerSlot* PlayerRegistry::FindByEntityVid(VID entity_vid) const {
    for (const PlayerSlot& slot : slots) {
        if (slot.entity_vid.has_value() && *slot.entity_vid == entity_vid) {
            return &slot;
        }
    }
    return nullptr;
}

PlayerSlot* PlayerRegistry::FindPrimaryLocal() {
    for (PlayerSlot& slot : slots) {
        if (slot.primary_local && slot.connection_kind == PlayerConnectionKind::Local) {
            return &slot;
        }
    }
    return nullptr;
}

const PlayerSlot* PlayerRegistry::FindPrimaryLocal() const {
    for (const PlayerSlot& slot : slots) {
        if (slot.primary_local && slot.connection_kind == PlayerConnectionKind::Local) {
            return &slot;
        }
    }
    return nullptr;
}

PlayerSlot& PlayerRegistry::EnsureLocalPlayer(
    PlayerId player_id,
    const std::string& display_name,
    bool primary
) {
    if (PlayerSlot* const existing = Find(player_id)) {
        existing->connection_kind = PlayerConnectionKind::Local;
        existing->connected = true;
        existing->display_name = display_name;
        if (primary) {
            for (PlayerSlot& slot : slots) {
                slot.primary_local = false;
            }
            existing->primary_local = true;
        }
        return *existing;
    }

    if (primary) {
        for (PlayerSlot& slot : slots) {
            slot.primary_local = false;
        }
    }

    PlayerSlot slot;
    slot.player_id = player_id;
    slot.connection_kind = PlayerConnectionKind::Local;
    slot.connected = true;
    slot.primary_local = primary;
    slot.display_name = display_name;
    slots.push_back(slot);
    return slots.back();
}

PlayerSlot& PlayerRegistry::EnsurePrimaryLocalPlayer() {
    return EnsureLocalPlayer(kPrimaryLocalPlayerId, "Player 1", true);
}

PlayerSlot& PlayerRegistry::EnsureRemotePlayer(PlayerId player_id, const std::string& display_name) {
    if (PlayerSlot* const existing = Find(player_id)) {
        existing->connection_kind = PlayerConnectionKind::Remote;
        existing->connected = true;
        existing->display_name = display_name;
        existing->primary_local = false;
        return *existing;
    }

    PlayerSlot slot;
    slot.player_id = player_id;
    slot.connection_kind = PlayerConnectionKind::Remote;
    slot.connected = true;
    slot.primary_local = false;
    slot.display_name = display_name;
    slots.push_back(slot);
    return slots.back();
}

void PlayerRegistry::Remove(PlayerId player_id) {
    slots.erase(
        std::remove_if(
            slots.begin(),
            slots.end(),
            [player_id](const PlayerSlot& slot) { return slot.player_id == player_id; }
        ),
        slots.end()
    );
}

void PlayerRegistry::AssignEntity(PlayerId player_id, VID entity_vid) {
    if (PlayerSlot* const slot = Find(player_id)) {
        slot->entity_vid = entity_vid;
    }
}

void PlayerRegistry::ClearEntityRefs() {
    for (PlayerSlot& slot : slots) {
        slot.entity_vid.reset();
    }
}

void PlayerRegistry::ClearEntityRef(VID entity_vid) {
    for (PlayerSlot& slot : slots) {
        if (slot.entity_vid.has_value() && *slot.entity_vid == entity_vid) {
            slot.entity_vid.reset();
        }
    }
}

std::optional<PlayerId> PlayerRegistry::FindPlayerIdForEntity(VID entity_vid) const {
    if (const PlayerSlot* const slot = FindByEntityVid(entity_vid)) {
        return slot->player_id;
    }
    return std::nullopt;
}

const PlayingInputs* PlayerRegistry::FindInputsForEntity(VID entity_vid) const {
    if (const PlayerSlot* const slot = FindByEntityVid(entity_vid)) {
        return &slot->inputs;
    }
    return nullptr;
}

const PlayingInputs* PlayerRegistry::FindInputsForPlayer(PlayerId player_id) const {
    if (const PlayerSlot* const slot = Find(player_id)) {
        return &slot->inputs;
    }
    return nullptr;
}

void PlayerRegistry::SetInputFrameForPlayer(
    PlayerId player_id,
    const PlayerInputFrame& input_frame
) {
    if (PlayerSlot* const slot = Find(player_id)) {
        slot->previous_input_frame = slot->input_frame;
        slot->input_frame = input_frame;
        slot->inputs = BuildPlayingInputs(slot->input_frame, slot->previous_input_frame);
        slot->immediate_inputs = slot->inputs;
    }
}

void PlayerRegistry::SetInputFrameAndInputsForPlayer(
    PlayerId player_id,
    const PlayerInputFrame& input_frame,
    const PlayingInputs& inputs,
    const PlayingInputs& immediate_inputs
) {
    if (PlayerSlot* const slot = Find(player_id)) {
        slot->previous_input_frame = slot->input_frame;
        slot->input_frame = input_frame;
        slot->inputs = inputs;
        slot->immediate_inputs = immediate_inputs;
    }
}

void PlayerRegistry::SetInputsForPlayer(
    PlayerId player_id,
    const PlayingInputs& inputs,
    const PlayingInputs& immediate_inputs
) {
    if (PlayerSlot* const slot = Find(player_id)) {
        slot->previous_input_frame = slot->input_frame;
        slot->input_frame = ToPlayerInputFrame(inputs);
        slot->inputs = inputs;
        slot->immediate_inputs = immediate_inputs;
    }
}

void PlayerRegistry::SetPrimaryLocalInputs(
    const PlayingInputs& inputs,
    const PlayingInputs& immediate_inputs
) {
    if (PlayerSlot* const slot = FindPrimaryLocal()) {
        slot->previous_input_frame = slot->input_frame;
        slot->input_frame = ToPlayerInputFrame(inputs);
        slot->inputs = inputs;
        slot->immediate_inputs = immediate_inputs;
    }
}

} // namespace splonks

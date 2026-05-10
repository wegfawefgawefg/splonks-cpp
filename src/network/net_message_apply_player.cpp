#include "network/net_message_apply_internal.hpp"

#include "entity.hpp"
#include "entity_tool_inventory.hpp"
#include "effects.hpp"
#include "network/net_session.hpp"
#include "state.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace splonks::network {

void ApplyPlayerStatePatchedMessage(
    NetSessionState& session,
    State& state,
    const PlayerStatePatchedMessage& payload
) {
    const std::optional<VID> vid = FindEntityVidForMessage(session, state, payload.player_entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const player = state.entity_manager.GetEntityMut(*vid);
    if (player == nullptr || !player->active) {
        return;
    }
    PlayerSlot* player_slot = state.players.FindByEntityVid(player->vid);
    if (player_slot == nullptr && payload.player_id != kInvalidPlayerId) {
        const bool local_player = payload.player_id == state.net_session.local_player_id;
        if (local_player) {
            player_slot = &state.players.EnsureLocalPlayer(payload.player_id, "Player", true);
        } else {
            player_slot = &state.players.EnsureRemotePlayer(
                payload.player_id,
                "Player " + std::to_string(payload.player_id)
            );
        }
        player_slot->entity_vid = player->vid;
    }
    if (player_slot == nullptr) {
        return;
    }
    if (player_slot->connection_kind == PlayerConnectionKind::Remote) {
        player_slot->connected = payload.connected != 0;
    }

    player->health = payload.health;
    player->money = payload.money;
    player->wanted = payload.wanted != 0;

    for (std::size_t i = 0; i < payload.tool_slots.size(); ++i) {
        ToolSlot& slot = state.entity_tools.EnsureToolSlot(player->vid, i);
        const PlayerStatePatchedToolSlot& patch = payload.tool_slots[i];
        slot.kind = patch.kind;
        slot.count = patch.count;
        slot.cooldown = patch.cooldown;
        slot.active = patch.active != 0;
    }

    player->effects.reset();
    const std::size_t effect_count =
        std::min<std::size_t>(payload.effect_count, payload.effects.size());
    if (effect_count > 0) {
        EntityEffects& effects = player->effects.emplace();
        effects.count = static_cast<std::uint8_t>(effect_count);
        for (std::size_t i = 0; i < effect_count; ++i) {
            const PlayerStatePatchedEffect& patch = payload.effects[i];
            effects.effects[i] = EffectInstance{
                .id = patch.id,
                .count = patch.count,
                .value = patch.value,
                .frames_remaining = patch.frames_remaining,
            };
        }
    }
}

} // namespace splonks::network

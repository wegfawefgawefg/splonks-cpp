#include "network/net_lobby_internal.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "network/net_entity_links.hpp"
#include "stage_spawning.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace splonks::network {

Vec2 GetPrimaryPlayerSpawnPos(const State& state) {
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal()) {
        if (primary->entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*primary->entity_vid)) {
                return entity->pos;
            }
        }
    }
    return Vec2::New(24.0F, 24.0F);
}

Vec2 GetRemoteSpawnPos(const State& state) {
    return GetPrimaryPlayerSpawnPos(state) + Vec2::New(16.0F, 0.0F);
}

Vec2 GetEntranceOrRemoteSpawnPos(const State& state) {
    return FindStageEntranceSpawnPos(state).value_or(GetRemoteSpawnPos(state));
}

NetRetainedPlayerState* FindRetainedPlayerState(State& state, PlayerId player_id) {
    for (NetRetainedPlayerState& retained : state.net_session.retained_players) {
        if (retained.player_id == player_id) {
            return &retained;
        }
    }
    return nullptr;
}

const NetRetainedPlayerState* FindRetainedPlayerState(const State& state, PlayerId player_id) {
    for (const NetRetainedPlayerState& retained : state.net_session.retained_players) {
        if (retained.player_id == player_id) {
            return &retained;
        }
    }
    return nullptr;
}

void CopyEntityEffectsToRetained(
    const Entity& entity,
    std::uint8_t& effect_count,
    std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount>& effects_out
) {
    effect_count = 0;
    if (const EntityEffects* const effects = entity.effects.get()) {
        effect_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(effects->count, effects_out.size())
        );
        for (std::size_t i = 0; i < effect_count; ++i) {
            const EffectInstance& effect = effects->effects[i];
            effects_out[i] = PlayerStatePatchedEffect{
                .id = effect.id,
                .count = effect.count,
                .value = effect.value,
                .frames_remaining = effect.frames_remaining,
            };
        }
    }
}

void RestoreRetainedEffects(
    Entity& entity,
    std::uint8_t effect_count,
    const std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount>& retained_effects
) {
    entity.effects.reset();
    const std::size_t count = std::min<std::size_t>(effect_count, retained_effects.size());
    if (count == 0) {
        return;
    }

    EntityEffects& effects = entity.effects.emplace();
    effects.count = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        const PlayerStatePatchedEffect& retained_effect = retained_effects[i];
        effects.effects[i] = EffectInstance{
            .id = retained_effect.id,
            .count = retained_effect.count,
            .value = retained_effect.value,
            .frames_remaining = retained_effect.frames_remaining,
        };
    }
}

NetRetainedAttachedEntityState CaptureRetainedAttachedEntity(
    const State& state,
    std::optional<VID> attached_vid
) {
    NetRetainedAttachedEntityState retained;
    if (!attached_vid.has_value()) {
        return retained;
    }

    const Entity* const attached = state.entity_manager.GetEntity(*attached_vid);
    if (attached == nullptr || !attached->active || IsPlayerLikeEntityType(attached->type_)) {
        return retained;
    }

    retained.valid = true;
    retained.entity_type = attached->type_;
    retained.pos = attached->pos;
    retained.vel = attached->vel;
    retained.acc = attached->acc;
    retained.size = attached->size;
    retained.rotation = attached->rotation;
    retained.counter_a = attached->counter_a;
    retained.counter_b = attached->counter_b;
    retained.counter_c = attached->counter_c;
    retained.counter_d = attached->counter_d;
    retained.health = attached->health;
    retained.money = attached->money;
    retained.facing = static_cast<std::uint8_t>(attached->facing == LeftOrRight::Right ? 1 : 0);
    retained.condition = static_cast<std::uint8_t>(attached->condition);
    CopyEntityEffectsToRetained(*attached, retained.effect_count, retained.effects);
    return retained;
}

void RemoveRetainedPlayerState(State& state, PlayerId player_id) {
    state.net_session.retained_players.erase(
        std::remove_if(
            state.net_session.retained_players.begin(),
            state.net_session.retained_players.end(),
            [player_id](const NetRetainedPlayerState& retained) {
                return retained.player_id == player_id;
            }
        ),
        state.net_session.retained_players.end()
    );
}

void StoreRetainedPlayerState(State& state, const PlayerSlot& slot, const Entity& player) {
    RemoveRetainedPlayerState(state, slot.player_id);

    NetRetainedPlayerState retained;
    retained.player_id = slot.player_id;
    retained.display_name = slot.display_name;
    retained.quest_id = state.stage.quest_id;
    retained.quest_stage_id = state.stage.quest_stage_id;
    retained.entity_type = player.type_;
    retained.last_pos = player.pos;
    retained.health = player.health;
    retained.money = player.money;
    retained.disconnected_frame = state.frame;
    retained.held_item = CaptureRetainedAttachedEntity(state, player.holding_vid);
    retained.back_item = CaptureRetainedAttachedEntity(state, player.back_vid);

    if (const EntityToolState* const tools = state.entity_tools.FindEntityToolState(player.vid)) {
        for (std::size_t i = 0; i < retained.tool_slots.size() && i < tools->slots.size(); ++i) {
            const ToolSlot& tool_slot = tools->slots[i];
            retained.tool_slots[i] = PlayerStatePatchedToolSlot{
                .kind = tool_slot.kind,
                .count = tool_slot.count,
                .cooldown = tool_slot.cooldown,
                .active = static_cast<std::uint8_t>(tool_slot.active ? 1 : 0),
            };
        }
    }

    CopyEntityEffectsToRetained(player, retained.effect_count, retained.effects);

    state.net_session.retained_players.push_back(retained);
}

void CleanupExpiredRetainedPlayerStates(State& state) {
    const std::uint64_t lifetime = state.net_session.retained_player_lifetime_frames;
    if (lifetime == 0) {
        return;
    }

    state.net_session.retained_players.erase(
        std::remove_if(
            state.net_session.retained_players.begin(),
            state.net_session.retained_players.end(),
            [&](const NetRetainedPlayerState& retained) {
                return state.frame > retained.disconnected_frame &&
                       state.frame - retained.disconnected_frame > lifetime;
            }
        ),
        state.net_session.retained_players.end()
    );
}

void DeactivateRetainedAttachedEntity(
    State& state,
    const NetRetainedAttachedEntityState& retained,
    std::optional<VID> attached_vid
) {
    if (!retained.valid || !attached_vid.has_value()) {
        return;
    }
    if (const Entity* const attached = state.entity_manager.GetEntity(*attached_vid);
        attached != nullptr && attached->active && attached->type_ == retained.entity_type &&
        !IsPlayerLikeEntityType(attached->type_)) {
        (void)world_ops::DeactivateEntity(state, attached->vid);
    }
}

bool IsRetainedReconnectMode(NetReconnectSpawnMode mode) {
    return mode == NetReconnectSpawnMode::RetainedAtEntrance ||
           mode == NetReconnectSpawnMode::RetainedAtLastPosition ||
           mode == NetReconnectSpawnMode::RetainedAtHost;
}

void ApplyRetainedAttachedEntityState(
    State& state,
    Entity& holder,
    const NetRetainedAttachedEntityState& retained,
    AttachmentMode mode,
    const Graphics& graphics
);

Vec2 ResolveReconnectSpawnPos(
    const State& state,
    const NetRetainedPlayerState* retained,
    std::size_t player_index
) {
    Vec2 pos = GetRemoteSpawnPos(state) + Vec2::New(static_cast<float>(player_index) * 8.0F, 0.0F);
    switch (state.net_session.reconnect_spawn_mode) {
    case NetReconnectSpawnMode::FreshAtEntrance:
    case NetReconnectSpawnMode::RetainedAtEntrance:
        pos = GetEntranceOrRemoteSpawnPos(state) + Vec2::New(static_cast<float>(player_index) * 8.0F, 0.0F);
        break;
    case NetReconnectSpawnMode::FreshAtHost:
    case NetReconnectSpawnMode::RetainedAtHost:
        pos = GetPrimaryPlayerSpawnPos(state) + Vec2::New(16.0F + static_cast<float>(player_index) * 8.0F, 0.0F);
        break;
    case NetReconnectSpawnMode::RetainedAtLastPosition:
        if (retained != nullptr) {
            pos = retained->last_pos;
        }
        break;
    }
    return pos;
}

void ApplyRetainedPlayerState(
    State& state,
    PlayerId player_id,
    const NetRetainedPlayerState& retained,
    const Vec2& spawn_pos,
    const Graphics& graphics
) {
    EnsureSpawnedPlayer(state, player_id, false, false, spawn_pos, graphics);
    PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->entity_vid.has_value()) {
        return;
    }

    Entity* const player = state.entity_manager.GetEntityMut(*slot->entity_vid);
    if (player == nullptr || !player->active) {
        return;
    }

    SetEntityAs(*player, retained.entity_type);
    player->pos = spawn_pos;
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
    player->health = retained.health;
    player->money = retained.money;
    player->held_by_vid.reset();
    player->holding_vid.reset();
    player->back_vid.reset();
    player->attachment_mode = AttachmentMode::None;
    player->stun_timer = 0;
    player->fall_timer = 0;
    player->coyote_time = 0;
    player->hang_side.reset();
    player->hang_count = 0;

    for (std::size_t i = 0; i < retained.tool_slots.size(); ++i) {
        const PlayerStatePatchedToolSlot& retained_tool = retained.tool_slots[i];
        ToolSlot& tool_slot = state.entity_tools.EnsureToolSlot(player->vid, i);
        tool_slot.kind = retained_tool.kind;
        tool_slot.count = retained_tool.count;
        tool_slot.cooldown = retained_tool.cooldown;
        tool_slot.active = retained_tool.active != 0;
    }

    RestoreRetainedEffects(*player, retained.effect_count, retained.effects);

    state.UpdateSidForEntity(player->vid.id, graphics);
    ApplyRetainedAttachedEntityState(state, *player, retained.back_item, AttachmentMode::Back, graphics);
    ApplyRetainedAttachedEntityState(state, *player, retained.held_item, AttachmentMode::Held, graphics);
}

void ApplyRetainedAttachedEntityState(
    State& state,
    Entity& holder,
    const NetRetainedAttachedEntityState& retained,
    AttachmentMode mode,
    const Graphics& graphics
) {
    if (!retained.valid) {
        return;
    }

    Entity* const attached = world_ops::SpawnEntity(
        state,
        retained.entity_type,
        [&](Entity& entity) {
            entity.pos = retained.pos;
            entity.vel = retained.vel;
            entity.acc = retained.acc;
            entity.size = retained.size;
            entity.rotation = retained.rotation;
            entity.counter_a = retained.counter_a;
            entity.counter_b = retained.counter_b;
            entity.counter_c = retained.counter_c;
            entity.counter_d = retained.counter_d;
            entity.health = retained.health;
            entity.money = retained.money;
            entity.facing = retained.facing != 0 ? LeftOrRight::Right : LeftOrRight::Left;
            entity.condition = static_cast<EntityCondition>(retained.condition);
            RestoreRetainedEffects(entity, retained.effect_count, retained.effects);
        }
    );
    if (attached == nullptr) {
        return;
    }

    if (mode == AttachmentMode::Back) {
        holder.back_vid = attached->vid;
        attached->held_by_vid = holder.vid;
        attached->attachment_mode = AttachmentMode::Back;
        attached->has_physics = false;
        attached->can_collide = false;
    } else {
        entities::common::AttachEntityAsHeld(holder, *attached);
    }

    entities::common::SyncEntityAttachments(holder.vid.id, state, graphics);
    world_ops::MarkEntityHeld(state, holder, *attached, mode);
    world_ops::PatchEntityState(state, holder, holder);
    world_ops::PatchEntityState(state, holder, *attached);
}

} // namespace splonks::network

#include "network/net_lobby_internal.hpp"

#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "network/net_ent_links.hpp"
#include "stage_spawning.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace splonks::network {

sim::Vec2 GetPrimaryPlayerSpawnPos(const State& state) {
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal()) {
        if (primary->ent_vid.has_value()) {
            if (const Ent* const ent = state.ents.GetEnt(*primary->ent_vid)) {
                return ent->pos;
            }
        }
    }
    return sim::PixelVec2(24, 24);
}

sim::Vec2 GetRemoteSpawnPos(const State& state) {
    return GetPrimaryPlayerSpawnPos(state) + sim::PixelVec2(16, 0);
}

sim::Vec2 GetEntranceOrRemoteSpawnPos(const State& state) {
    if (const std::optional<Vec2> entrance_pos = FindStageEntranceSpawnPos(state)) {
        return sim::ToSimVec2(*entrance_pos);
    }
    return GetRemoteSpawnPos(state);
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

void CopyEntEffectsToRetained(
    const Ent& ent,
    std::uint8_t& effect_count,
    std::array<NetRetainedEffect, kNetRetainedEffectCount>& effects_out
) {
    effect_count = 0;
    if (const EntEffects* const effects = ent.effects.get()) {
        effect_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(effects->count, effects_out.size())
        );
        for (std::size_t i = 0; i < effect_count; ++i) {
            const EffectInstance& effect = effects->effects[i];
            effects_out[i] = NetRetainedEffect{
                .id = effect.id,
                .count = effect.count,
                .value = effect.value,
                .frames_remaining = effect.frames_remaining,
            };
        }
    }
}

void RestoreRetainedEffects(
    Ent& ent,
    std::uint8_t effect_count,
    const std::array<NetRetainedEffect, kNetRetainedEffectCount>& retained_effects
) {
    ent.effects.reset();
    const std::size_t count = std::min<std::size_t>(effect_count, retained_effects.size());
    if (count == 0) {
        return;
    }

    EntEffects& effects = ent.effects.emplace();
    effects.count = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        const NetRetainedEffect& retained_effect = retained_effects[i];
        effects.effects[i] = EffectInstance{
            .id = retained_effect.id,
            .count = retained_effect.count,
            .value = retained_effect.value,
            .frames_remaining = retained_effect.frames_remaining,
        };
    }
}

NetRetainedAttachedEntState CaptureRetainedAttachedEnt(
    const State& state,
    std::optional<VID> attached_vid
) {
    NetRetainedAttachedEntState retained;
    if (!attached_vid.has_value()) {
        return retained;
    }

    const Ent* const attached = state.ents.GetEnt(*attached_vid);
    if (attached == nullptr || !attached->active || IsPlayerLikeEntType(attached->type_)) {
        return retained;
    }

    retained.valid = true;
    retained.ent_type = attached->type_;
    retained.pos = attached->pos;
    retained.vel = attached->vel;
    retained.acc = attached->acc;
    retained.size = attached->size;
    retained.rotation = attached->rotation;
    retained.counter_a = sim::ToSimScalar(attached->counter_a);
    retained.counter_b = sim::ToSimScalar(attached->counter_b);
    retained.counter_c = sim::ToSimScalar(attached->counter_c);
    retained.counter_d = sim::ToSimScalar(attached->counter_d);
    retained.health = attached->health;
    retained.money = attached->money;
    retained.facing = static_cast<std::uint8_t>(attached->facing == Side::Right ? 1 : 0);
    retained.condition = static_cast<std::uint8_t>(attached->condition);
    CopyEntEffectsToRetained(*attached, retained.effect_count, retained.effects);
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

void StoreRetainedPlayerState(State& state, const PlayerSlot& slot, const Ent& player) {
    RemoveRetainedPlayerState(state, slot.player_id);

    NetRetainedPlayerState retained;
    retained.player_id = slot.player_id;
    retained.display_name = slot.display_name;
    retained.quest_id = state.stage.quest_id;
    retained.quest_stage_id = state.stage.quest_stage_id;
    retained.ent_type = player.type_;
    retained.last_pos = player.pos;
    retained.health = player.health;
    retained.money = player.money;
    retained.disconnected_frame = state.frame;
    retained.held_item = CaptureRetainedAttachedEnt(state, player.holding_vid);
    retained.back_item = CaptureRetainedAttachedEnt(state, player.back_vid);

    if (const EntToolState* const tools = state.ent_tools.FindEntToolState(player.vid)) {
        for (std::size_t i = 0; i < retained.tool_slots.size() && i < tools->slots.size(); ++i) {
            const ToolSlot& tool_slot = tools->slots[i];
            retained.tool_slots[i] = NetRetainedToolSlot{
                .kind = tool_slot.kind,
                .count = tool_slot.count,
                .cooldown = tool_slot.cooldown,
                .active = static_cast<std::uint8_t>(tool_slot.active ? 1 : 0),
            };
        }
    }

    CopyEntEffectsToRetained(player, retained.effect_count, retained.effects);

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

void DeactivateRetainedAttachedEnt(
    State& state,
    const NetRetainedAttachedEntState& retained,
    std::optional<VID> attached_vid
) {
    if (!retained.valid || !attached_vid.has_value()) {
        return;
    }
    if (const Ent* const attached = state.ents.GetEnt(*attached_vid);
        attached != nullptr && attached->active && attached->type_ == retained.ent_type &&
        !IsPlayerLikeEntType(attached->type_)) {
        (void)world_ops::DeactivateEnt(state, attached->vid);
    }
}

bool IsRetainedReconnectMode(NetReconnectSpawnMode mode) {
    return mode == NetReconnectSpawnMode::RetainedAtEntrance ||
           mode == NetReconnectSpawnMode::RetainedAtLastPosition ||
           mode == NetReconnectSpawnMode::RetainedAtHost;
}

void ApplyRetainedAttachedEntState(
    State& state,
    Ent& holder,
    const NetRetainedAttachedEntState& retained,
    AttachMode mode,
    const Graphics& graphics
);

sim::Vec2 ResolveReconnectSpawnPos(
    const State& state,
    const NetRetainedPlayerState* retained,
    std::size_t player_index
) {
    const sim::Vec2 player_offset = sim::PixelVec2(static_cast<int>(player_index) * 8, 0);
    sim::Vec2 pos = GetRemoteSpawnPos(state) + player_offset;
    switch (state.net_session.reconnect_spawn_mode) {
    case NetReconnectSpawnMode::FreshAtEntrance:
    case NetReconnectSpawnMode::RetainedAtEntrance:
        pos = GetEntranceOrRemoteSpawnPos(state) + player_offset;
        break;
    case NetReconnectSpawnMode::FreshAtHost:
    case NetReconnectSpawnMode::RetainedAtHost:
        pos = GetPrimaryPlayerSpawnPos(state) + sim::PixelVec2(16, 0) + player_offset;
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
    sim::Vec2 spawn_pos,
    const Graphics& graphics
) {
    EnsureSpawnedPlayer(state, player_id, false, false, spawn_pos, graphics);
    PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->ent_vid.has_value()) {
        return;
    }

    Ent* const player = state.ents.GetEntMut(*slot->ent_vid);
    if (player == nullptr || !player->active) {
        return;
    }

    SetEntAs(*player, retained.ent_type);
    player->pos = spawn_pos;
    player->vel = sim::Vec2::zero();
    player->acc = sim::Vec2::zero();
    player->health = retained.health;
    player->money = retained.money;
    player->held_by_vid.reset();
    player->holding_vid.reset();
    player->back_vid.reset();
    player->attach_mode = AttachMode::None;
    player->stun_timer = 0;
    player->fall_timer = 0;
    player->coyote_time = 0;
    player->hang_side.reset();
    player->hang_count = 0;

    for (std::size_t i = 0; i < retained.tool_slots.size(); ++i) {
        const NetRetainedToolSlot& retained_tool = retained.tool_slots[i];
        ToolSlot& tool_slot = state.ent_tools.EnsureToolSlot(player->vid, i);
        tool_slot.kind = retained_tool.kind;
        tool_slot.count = retained_tool.count;
        tool_slot.cooldown = retained_tool.cooldown;
        tool_slot.active = retained_tool.active != 0;
    }

    RestoreRetainedEffects(*player, retained.effect_count, retained.effects);

    state.UpdateSidForEnt(player->vid.id, graphics);
    ApplyRetainedAttachedEntState(state, *player, retained.back_item, AttachMode::Back, graphics);
    ApplyRetainedAttachedEntState(state, *player, retained.held_item, AttachMode::Held, graphics);
}

void ApplyRetainedAttachedEntState(
    State& state,
    Ent& holder,
    const NetRetainedAttachedEntState& retained,
    AttachMode mode,
    const Graphics& graphics
) {
    if (!retained.valid) {
        return;
    }

    Ent* const attached = world_ops::SpawnEnt(
        state,
        retained.ent_type,
        [&](Ent& ent) {
            ent.pos = retained.pos;
            ent.vel = retained.vel;
            ent.acc = retained.acc;
            ent.size = retained.size;
            ent.rotation = retained.rotation;
            ent.counter_a = sim::ToRenderScalar(retained.counter_a);
            ent.counter_b = sim::ToRenderScalar(retained.counter_b);
            ent.counter_c = sim::ToRenderScalar(retained.counter_c);
            ent.counter_d = sim::ToRenderScalar(retained.counter_d);
            ent.health = retained.health;
            ent.money = retained.money;
            ent.facing = retained.facing != 0 ? Side::Right : Side::Left;
            ent.condition = static_cast<EntCondition>(retained.condition);
            RestoreRetainedEffects(ent, retained.effect_count, retained.effects);
        }
    );
    if (attached == nullptr) {
        return;
    }

    if (mode == AttachMode::Back) {
        holder.back_vid = attached->vid;
        attached->held_by_vid = holder.vid;
        attached->attach_mode = AttachMode::Back;
        attached->has_physics = false;
        attached->can_collide = false;
    } else {
        ents::common::AttachEntAsHeld(holder, *attached);
    }

    ents::common::SyncEntAttachs(holder.vid.id, state, graphics);
    (void)mode;
}

} // namespace splonks::network

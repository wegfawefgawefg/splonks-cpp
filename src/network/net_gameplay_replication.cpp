#include "network/net_gameplay_replication.hpp"

#include "entity.hpp"
#include "effects.hpp"
#include "entity/replicated_runtime_flags.hpp"
#include "entity_tool_inventory.hpp"
#include "gameplay_authority.hpp"
#include "network/net_entity_links.hpp"
#include "network/net_message.hpp"
#include "network/net_session.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace splonks::network {

namespace {

bool ShouldReplicateInteractionSourceMessage(const State& state, VID source_vid) {
    return state.net_session.role != NetRole::Offline &&
           HasLocalGameplayAuthorityForInteractionSource(state, source_vid);
}

NetEntityId GetReplicatedEntityLinkId(State& state, const std::optional<VID>& vid) {
    if (!vid.has_value()) {
        return kInvalidNetEntityId;
    }
    const Entity* const linked_entity = state.entity_manager.GetEntity(*vid);
    if (linked_entity == nullptr || !linked_entity->active) {
        return kInvalidNetEntityId;
    }
    return GetOrAssignReplicatedEntityId(state, *vid);
}

bool IsPlayerBodyDamageBetweenPlayers(const State& state, const GameplayEntityDamaged& message) {
    if (!message.source_vid.has_value() ||
        (message.damage_type != DamageType::Attack &&
         message.damage_type != DamageType::JumpOn)) {
        return false;
    }

    const Entity* const target = state.entity_manager.GetEntity(message.entity_vid);
    const Entity* const source = state.entity_manager.GetEntity(*message.source_vid);
    return target != nullptr && source != nullptr &&
           IsPlayerLikeEntityType(target->type_) &&
           IsPlayerLikeEntityType(source->type_);
}

template <typename Payload>
void CopyEntityEffectsToPayload(const Entity* entity, Payload& payload) {
    if (entity == nullptr) {
        return;
    }
    const EntityEffects* const effects = entity->effects.get();
    if (effects == nullptr) {
        return;
    }
    payload.effect_count = static_cast<std::uint8_t>(
        std::min<std::size_t>(effects->count, payload.effects.size())
    );
    for (std::size_t i = 0; i < payload.effect_count; ++i) {
        const EffectInstance& effect = effects->effects[i];
        payload.effects[i] = EntityReplicatedEffect{
            .id = effect.id,
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
}

} // namespace

void ReplicateEntitySpawned(State& state, const GameplayEntitySpawned& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetEntityId net_id = state.net_session.FindNetEntityId(message.entity_vid)
        .value_or(kInvalidNetEntityId);
    if (net_id == kInvalidNetEntityId) {
        net_id = state.net_session.AllocateLocalEntityId();
        state.net_session.LinkEntity(net_id, message.entity_vid);
    }
    state.net_session.SetEntityOwner(net_id, std::nullopt);

    NetEntityId held_by_id = kInvalidNetEntityId;
    if (message.held_by_vid.has_value()) {
        held_by_id = GetOrAssignReplicatedEntityId(state, *message.held_by_vid);
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::EntitySpawned;
    EntitySpawnedMessage payload{
        .entity_id = net_id,
        .entity_type = message.entity_type,
        .held_by_id = held_by_id,
        .pos = message.pos,
        .vel = message.vel,
        .acc = message.acc,
        .size = message.size,
        .owner = NetEntityOwner::Coordinator(),
        .counter_a = message.counter_a,
        .counter_b = message.counter_b,
        .light_strength = message.light_strength,
        .light_color = message.light_color,
        .light_radius = message.light_radius,
        .movement_flags = message.movement_flags,
        .use_pressed = message.use_pressed,
        .animate = message.animate,
        .animation_loop = message.animation_loop,
        .animation_finished = message.animation_finished,
        .animation_id = message.animation_id,
        .animation_frame = message.animation_frame,
        .animation_time = message.animation_time,
        .animation_speed = message.animation_speed,
    };
    CopyEntityEffectsToPayload(state.entity_manager.GetEntity(message.entity_vid), payload);
    net_message.payload = payload;
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateEntityDeactivated(
    State& state,
    const GameplayEntityDeactivated& message
) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const std::optional<NetEntityId> entity_id =
        state.net_session.FindNetEntityId(message.entity_vid);
    if (!entity_id.has_value()) {
        return;
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::EntityDeactivated;
    net_message.payload = EntityIdMessage{
        .entity_id = *entity_id,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateEntityHeld(State& state, const GameplayEntityHeld& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const NetEntityId held_id = GetOrAssignReplicatedEntityId(state, message.held_vid);

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::EntityHeld;
    net_message.payload = EntityHeldMessage{
        .holder_id = GetOrAssignReplicatedEntityId(state, message.holder_vid),
        .held_id = held_id,
        .attachment_mode = message.attachment_mode,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateEntityDropped(State& state, const GameplayEntityDropped& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::EntityDropped;
    net_message.payload = EntityDroppedMessage{
        .entity_id = GetOrAssignReplicatedEntityId(state, message.entity_vid),
        .dropped_by_id = message.dropped_by_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *message.dropped_by_vid)
            : kInvalidNetEntityId,
        .pos = message.pos,
        .vel = message.vel,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateEntityThrown(State& state, const GameplayEntityThrown& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const NetEntityId entity_id = GetOrAssignReplicatedEntityId(state, message.entity_vid);
    if (state.players.FindByEntityVid(message.entity_vid) == nullptr) {
        state.net_session.SetEntityOwner(entity_id, std::nullopt);
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::EntityThrown;
    net_message.payload = EntityThrownMessage{
        .entity_id = entity_id,
        .pos = message.pos,
        .vel = message.vel,
        .thrower_id = GetOrAssignReplicatedEntityId(state, message.thrower_vid),
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateEntityDamaged(State& state, const GameplayEntityDamaged& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    if (IsPlayerBodyDamageBetweenPlayers(state, message)) {
        return;
    }

    const NetEntityId entity_id = GetOrAssignReplicatedEntityId(state, message.entity_vid);

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::EntityDamaged;
    net_message.payload = EntityDamagedMessage{
        .entity_id = entity_id,
        .source_entity_id = message.source_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *message.source_vid)
            : kInvalidNetEntityId,
        .amount = message.amount,
        .remaining_health = message.remaining_health,
        .pos = message.pos,
        .vel = message.vel,
        .acc = message.acc,
        .fall_timer = message.fall_timer,
        .stun_timer = message.stun_timer,
        .projectile_contact_timer = message.projectile_contact_timer,
        .condition = message.condition,
        .grounded = message.grounded,
        .animate = message.animate,
        .animation_id = message.animation_id,
        .animation_frame = message.animation_frame,
        .animation_time = message.animation_time,
        .animation_speed = message.animation_speed,
        .damage_type = message.damage_type,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateEntityStatePatched(
    State& state,
    const GameplayEntityStatePatched& message
) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const NetEntityId entity_id = GetOrAssignReplicatedEntityId(state, message.entity_vid);

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalTransientMessageHeader(state.frame);
    net_message.type = NetMessageType::EntityStatePatched;
    EntityStatePatchedMessage payload{
        .entity_id = entity_id,
        .source_entity_id = GetOrAssignReplicatedEntityId(state, message.source_vid),
        .entity_a_id = GetReplicatedEntityLinkId(state, message.entity_a_vid),
        .entity_b_id = GetReplicatedEntityLinkId(state, message.entity_b_vid),
        .entity_c_id = GetReplicatedEntityLinkId(state, message.entity_c_vid),
        .entity_d_id = GetReplicatedEntityLinkId(state, message.entity_d_vid),
        .holding_id = GetReplicatedEntityLinkId(state, message.holding_vid),
        .held_by_id = GetReplicatedEntityLinkId(state, message.held_by_vid),
        .back_id = GetReplicatedEntityLinkId(state, message.back_vid),
        .pos = message.pos,
        .vel = message.vel,
        .acc = message.acc,
        .size = message.size,
        .counter_a = message.counter_a,
        .counter_b = message.counter_b,
        .counter_c = message.counter_c,
        .counter_d = message.counter_d,
        .threshold_a = message.threshold_a,
        .threshold_b = message.threshold_b,
        .point_a = message.point_a,
        .point_b = message.point_b,
        .point_c = message.point_c,
        .point_d = message.point_d,
        .health = message.health,
        .coyote_time = message.coyote_time,
        .fall_timer = message.fall_timer,
        .stun_timer = message.stun_timer,
        .projectile_contact_timer = message.projectile_contact_timer,
        .light_strength = message.light_strength,
        .light_color = message.light_color,
        .light_radius = message.light_radius,
        .rotation = message.rotation,
        .condition = message.condition,
        .grounded = message.grounded,
        .active = message.active,
        .has_physics = message.has_physics,
        .can_collide = message.can_collide,
        .can_apply_projectile_contact = message.can_apply_projectile_contact,
        .damage_vulnerability = message.damage_vulnerability,
        .facing = message.facing,
        .ai_state = message.ai_state,
        .wanted = message.wanted,
        .holding = message.holding,
        .render_enabled = message.render_enabled,
        .attachment_mode = message.attachment_mode,
        .draw_layer = message.draw_layer,
        .movement_flags = message.movement_flags,
        .money = message.money,
        .stage_exit_id = message.stage_exit_id,
        .runtime_flags = message.runtime_flags,
        .buyable_active = message.buyable_active,
        .buyable_display_quantity = message.buyable_display_quantity,
        .buyable_display_icon_animation_id = message.buyable_display_icon_animation_id,
        .buyable_shop_owner_id = GetReplicatedEntityLinkId(state, message.buyable_shop_owner_vid),
        .animate = message.animate,
        .animation_loop = message.animation_loop,
        .animation_finished = message.animation_finished,
        .animation_id = message.animation_id,
        .animation_frame = message.animation_frame,
        .animation_time = message.animation_time,
        .animation_speed = message.animation_speed,
    };
    CopyEntityEffectsToPayload(state.entity_manager.GetEntity(message.entity_vid), payload);
    net_message.payload = payload;
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicatePlayerStatePatched(
    State& state,
    const GameplayPlayerStatePatched& message
) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const Entity* const player = state.entity_manager.GetEntity(message.player_vid);
    if (player == nullptr || !player->active) {
        return;
    }
    const PlayerSlot* const player_slot = state.players.FindByEntityVid(player->vid);
    if (player_slot == nullptr) {
        return;
    }

    PlayerStatePatchedMessage payload{
        .player_entity_id = GetOrAssignReplicatedEntityId(state, player->vid),
        .player_id = player_slot->player_id,
        .health = player->health,
        .money = player->money,
        .wanted = static_cast<std::uint8_t>(player->wanted ? 1 : 0),
        .connected = static_cast<std::uint8_t>(player_slot->connected ? 1 : 0),
    };

    if (const EntityToolState* const tools = state.entity_tools.FindEntityToolState(player->vid)) {
        for (std::size_t i = 0; i < payload.tool_slots.size() && i < tools->slots.size(); ++i) {
            const ToolSlot& slot = tools->slots[i];
            payload.tool_slots[i] = PlayerStatePatchedToolSlot{
                .kind = slot.kind,
                .count = slot.count,
                .cooldown = slot.cooldown,
                .active = static_cast<std::uint8_t>(slot.active ? 1 : 0),
            };
        }
    }

    if (const EntityEffects* const effects = player->effects.get()) {
        payload.effect_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(effects->count, payload.effects.size())
        );
        for (std::size_t i = 0; i < payload.effect_count; ++i) {
            const EffectInstance& effect = effects->effects[i];
            payload.effects[i] = PlayerStatePatchedEffect{
                .id = effect.id,
                .count = effect.count,
                .value = effect.value,
                .frames_remaining = effect.frames_remaining,
            };
        }
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::PlayerStatePatched;
    net_message.payload = payload;
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateRunStatePatched(State& state, bool include_snapshot_fingerprint) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const std::uint64_t snapshot_fingerprint = include_snapshot_fingerprint
        ? ComputeNetworkStateFingerprint(state).value
        : 0U;

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::RunStatePatched;
    net_message.payload = RunStatePatchedMessage{
        .quest_id = state.quest_state.quest_id,
        .frame = state.frame,
        .stage_frame = state.stage_frame,
        .depth = state.depth,
        .points = state.points,
        .deaths = state.deaths,
        .stage_type = static_cast<std::uint32_t>(state.stage.stage_type),
        .quest_level_number = state.stage.quest_level_number,
        .generation_seed = state.stage.generation_seed.value_or(0U),
        .tile_change_generation = state.stage.tile_change_generation,
        .stage_gravity = state.stage.gravity,
        .border_left_tile = state.stage.border.left.tile,
        .border_right_tile = state.stage.border.right.tile,
        .border_top_tile = state.stage.border.top.tile,
        .border_bottom_tile = state.stage.border.bottom.tile,
        .void_death_y = state.stage.border.void_death_y.value_or(0),
        .has_generation_seed =
            static_cast<std::uint8_t>(state.stage.generation_seed.has_value() ? 1 : 0),
        .border_wrap_x = static_cast<std::uint8_t>(state.stage.border.wrap_x ? 1 : 0),
        .border_wrap_y = static_cast<std::uint8_t>(state.stage.border.wrap_y ? 1 : 0),
        .has_void_death_y =
            static_cast<std::uint8_t>(state.stage.border.void_death_y.has_value() ? 1 : 0),
        .camera_clamp_enabled =
            static_cast<std::uint8_t>(state.stage.camera_clamp_enabled ? 1 : 0),
        .wrap_transform_active =
            static_cast<std::uint8_t>(state.stage.wrap_transform_active ? 1 : 0),
        .game_over = static_cast<std::uint8_t>(state.game_over ? 1 : 0),
        .win = static_cast<std::uint8_t>(state.win ? 1 : 0),
        .wrap_padding_tiles = static_cast<std::uint16_t>(
            std::min<unsigned int>(
                state.stage.wrap_padding_tiles,
                std::numeric_limits<std::uint16_t>::max()
            )
        ),
        .wrap_core_origin_x = state.stage.wrap_core_origin_tiles.x,
        .wrap_core_origin_y = state.stage.wrap_core_origin_tiles.y,
        .wrap_core_size_x = state.stage.wrap_core_size_tiles.x,
        .wrap_core_size_y = state.stage.wrap_core_size_tiles.y,
        .classic_made_black_market =
            static_cast<std::uint8_t>(state.quest_state.classic.made_black_market ? 1 : 0),
        .classic_made_udjat_eye =
            static_cast<std::uint8_t>(state.quest_state.classic.made_udjat_eye ? 1 : 0),
        .classic_has_udjat_eye =
            static_cast<std::uint8_t>(state.quest_state.classic.has_udjat_eye ? 1 : 0),
        .classic_made_moai =
            static_cast<std::uint8_t>(state.quest_state.classic.made_moai ? 1 : 0),
        .classic_has_hedjet =
            static_cast<std::uint8_t>(state.quest_state.classic.has_hedjet ? 1 : 0),
        .classic_has_sceptre =
            static_cast<std::uint8_t>(state.quest_state.classic.has_sceptre ? 1 : 0),
        .classic_has_book_of_dead =
            static_cast<std::uint8_t>(state.quest_state.classic.has_book_of_dead ? 1 : 0),
        .sac_altar_favor = state.sac_altar_favor,
        .sac_altar_reward_tier = state.sac_altar_reward_tier,
        .has_snapshot_fingerprint =
            static_cast<std::uint8_t>(include_snapshot_fingerprint ? 1 : 0),
        .snapshot_fingerprint = snapshot_fingerprint,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateTileBroken(State& state, const GameplayTileBroken& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::TileBroken;
    net_message.payload = TileBrokenMessage{
        .tile_pos = message.tile_pos,
        .source_entity_id = kInvalidNetEntityId,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateTileChanged(State& state, const GameplayTileChanged& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::TileChanged;
    net_message.payload = TileChangedMessage{
        .tile_pos = message.tile_pos,
        .tile = message.tile,
        .rotation = message.rotation,
        .layer = message.layer == GameplayTileLayer::Backwall
            ? NetTileLayer::Backwall
            : NetTileLayer::Foreground,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateStageLightAdded(State& state, const GameplayStageLightAdded& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::StageLightAdded;
    net_message.payload = StageLightAddedMessage{
        .light_id = static_cast<std::uint64_t>(message.light_vid.id),
        .tile_pos = message.tile_pos,
        .radius = static_cast<std::int32_t>(message.radius),
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicateStageLightRemoved(State& state, const GameplayStageLightRemoved& message) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::StageLightRemoved;
    net_message.payload = StageLightRemovedMessage{
        .light_id = static_cast<std::uint64_t>(message.light_vid.id),
    };
    state.net_session.EnqueueNetMessage(net_message);
}

void ReplicatePresentationCommand(State& state, const PresentationCommand& command) {
    if (state.net_session.role == NetRole::Offline) {
        return;
    }
    if (state.net_session.role == NetRole::Peer &&
        command.source_vid.has_value() &&
        !ShouldReplicateInteractionSourceMessage(state, *command.source_vid)) {
        return;
    }

    NetMessage net_message;
    net_message.header = state.net_session.MakeLocalMessageHeader(state.frame);
    net_message.type = NetMessageType::PresentationCommand;
    net_message.payload = PresentationCommandMessage{
        .kind = static_cast<std::uint16_t>(command.kind),
        .effect_id = static_cast<std::uint16_t>(command.effect_id),
        .audio_asset_id = command.audio_asset_id,
        .source_entity_id = command.source_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *command.source_vid)
            : kInvalidNetEntityId,
        .target_entity_id = command.target_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *command.target_vid)
            : kInvalidNetEntityId,
        .source_pos = command.source_pos,
        .target_pos = command.target_pos,
        .direction_x = command.direction.x,
        .direction_y = command.direction.y,
        .effect_count = command.effect_count,
        .effect_scale = command.effect_scale,
        .entity_shake_amount = command.entity_shake_amount,
        .foreground_shake_amount = command.foreground_shake_amount,
        .background_shake_amount = command.background_shake_amount,
        .area_entity_shake_amount = command.area_entity_shake_amount,
        .shake_radius_tiles = command.shake_radius_tiles,
        .light_strength = command.light_strength,
        .light_color = command.light_color,
        .light_radius = static_cast<std::int32_t>(command.light_radius),
        .light_lifetime_frames = command.light_lifetime_frames,
    };
    state.net_session.EnqueueNetMessage(net_message);
}

} // namespace splonks::network

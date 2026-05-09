#include "network/net_lobby_internal.hpp"

#include "entities/common/common.hpp"
#include "network/net_protocol.hpp"
#include "stage_spawning.hpp"

#include <algorithm>
#include <cstdint>

namespace splonks::network {

namespace {

constexpr std::uint16_t kPlayerSnapshotInputLeft = 1U << 0U;
constexpr std::uint16_t kPlayerSnapshotInputRight = 1U << 1U;
constexpr std::uint16_t kPlayerSnapshotInputUp = 1U << 2U;
constexpr std::uint16_t kPlayerSnapshotInputDown = 1U << 3U;
constexpr std::uint16_t kPlayerSnapshotInputJump = 1U << 4U;
constexpr std::uint16_t kPlayerSnapshotInputRun = 1U << 5U;
constexpr std::uint16_t kPlayerSnapshotInputUseBack = 1U << 6U;
constexpr std::uint16_t kPlayerSnapshotInputEquip = 1U << 7U;
constexpr std::uint16_t kPlayerSnapshotInputPickupDrop = 1U << 8U;
constexpr std::uint16_t kPlayerSnapshotInputStop = 1U << 9U;
constexpr std::uint16_t kPlayerSnapshotInputBomb = 1U << 10U;
constexpr std::uint16_t kPlayerSnapshotInputRope = 1U << 11U;
constexpr std::uint16_t kPlayerSnapshotInputAttack = 1U << 12U;
constexpr std::uint16_t kPlayerSnapshotInputBuy = 1U << 13U;
constexpr std::uint16_t kPlayerSnapshotInputEmoteUp = 1U << 14U;
constexpr std::uint16_t kPlayerSnapshotInputEmoteDown = 1U << 15U;

std::uint8_t EncodeHangSide(const std::optional<LeftOrRight>& hang_side) {
    if (!hang_side.has_value()) {
        return 0;
    }
    return *hang_side == LeftOrRight::Left ? 1 : 2;
}

std::optional<LeftOrRight> DecodeHangSide(std::uint8_t hang_side) {
    if (hang_side == 1) {
        return LeftOrRight::Left;
    }
    if (hang_side == 2) {
        return LeftOrRight::Right;
    }
    return std::nullopt;
}

NetEntityId FindExistingEntityLinkId(const State& state, const std::optional<VID>& vid) {
    if (!vid.has_value()) {
        return kInvalidNetEntityId;
    }
    const Entity* const entity = state.entity_manager.GetEntity(*vid);
    if (entity == nullptr || !entity->active) {
        return kInvalidNetEntityId;
    }
    return state.net_session.FindNetEntityId(*vid).value_or(kInvalidNetEntityId);
}

std::uint16_t BuildInputFlags(const PlayingInputs& inputs) {
    std::uint16_t flags = 0;
    if (inputs.left.down) {
        flags |= kPlayerSnapshotInputLeft;
    }
    if (inputs.right.down) {
        flags |= kPlayerSnapshotInputRight;
    }
    if (inputs.up.down) {
        flags |= kPlayerSnapshotInputUp;
    }
    if (inputs.down.down) {
        flags |= kPlayerSnapshotInputDown;
    }
    if (inputs.jump.down) {
        flags |= kPlayerSnapshotInputJump;
    }
    if (inputs.run.down) {
        flags |= kPlayerSnapshotInputRun;
    }
    if (inputs.use_button.down) {
        flags |= kPlayerSnapshotInputUseBack;
    }
    if (inputs.equip_button.down) {
        flags |= kPlayerSnapshotInputEquip;
    }
    if (inputs.pick_up_drop.down) {
        flags |= kPlayerSnapshotInputPickupDrop;
    }
    if (inputs.stop.down) {
        flags |= kPlayerSnapshotInputStop;
    }
    if (inputs.bomb.down) {
        flags |= kPlayerSnapshotInputBomb;
    }
    if (inputs.rope.down) {
        flags |= kPlayerSnapshotInputRope;
    }
    if (inputs.attack.down) {
        flags |= kPlayerSnapshotInputAttack;
    }
    if (inputs.buy_button.down) {
        flags |= kPlayerSnapshotInputBuy;
    }
    if (inputs.emote_up.down) {
        flags |= kPlayerSnapshotInputEmoteUp;
    }
    if (inputs.emote_down.down) {
        flags |= kPlayerSnapshotInputEmoteDown;
    }
    return flags;
}

ButtonState BuildButtonStateFromNetwork(bool down, const ButtonState& previous) {
    return ButtonState{
        .down = down,
        .pressed = down && !previous.down,
        .released = !down && previous.down,
    };
}

void ApplyInputFlags(PlayerSlot& slot, std::uint16_t flags) {
    slot.inputs.left = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputLeft) != 0,
        slot.inputs.left
    );
    slot.inputs.right = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputRight) != 0,
        slot.inputs.right
    );
    slot.inputs.up = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputUp) != 0,
        slot.inputs.up
    );
    slot.inputs.down = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputDown) != 0,
        slot.inputs.down
    );
    slot.inputs.jump = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputJump) != 0,
        slot.inputs.jump
    );
    slot.inputs.run = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputRun) != 0,
        slot.inputs.run
    );
    slot.inputs.use_button = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputUseBack) != 0,
        slot.inputs.use_button
    );
    slot.inputs.equip_button = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputEquip) != 0,
        slot.inputs.equip_button
    );
    slot.inputs.pick_up_drop = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputPickupDrop) != 0,
        slot.inputs.pick_up_drop
    );
    slot.inputs.stop = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputStop) != 0,
        slot.inputs.stop
    );
    slot.inputs.bomb = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputBomb) != 0,
        slot.inputs.bomb
    );
    slot.inputs.rope = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputRope) != 0,
        slot.inputs.rope
    );
    slot.inputs.attack = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputAttack) != 0,
        slot.inputs.attack
    );
    slot.inputs.buy_button = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputBuy) != 0,
        slot.inputs.buy_button
    );
    slot.inputs.emote_up = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputEmoteUp) != 0,
        slot.inputs.emote_up
    );
    slot.inputs.emote_down = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputEmoteDown) != 0,
        slot.inputs.emote_down
    );
    slot.immediate_inputs = slot.inputs;
}

std::optional<PlayerSnapshotEntry> MakeSnapshotForSlot(State& state, const PlayerSlot& slot) {
    if (!slot.entity_vid.has_value()) {
        return std::nullopt;
    }
    const Entity* const entity = state.entity_manager.GetEntity(*slot.entity_vid);
    if (entity == nullptr || !entity->active) {
        return std::nullopt;
    }

    PlayerSnapshotEntry snapshot;
    snapshot.player_id = slot.player_id;
    snapshot.pos_x = entity->pos.x;
    snapshot.pos_y = entity->pos.y;
    snapshot.vel_x = entity->vel.x;
    snapshot.vel_y = entity->vel.y;
    snapshot.acc_x = entity->acc.x;
    snapshot.acc_y = entity->acc.y;
    snapshot.size_x = entity->size.x;
    snapshot.size_y = entity->size.y;
    snapshot.rotation = entity->rotation;
    snapshot.health = entity->health;
    snapshot.coyote_time = entity->coyote_time;
    snapshot.fall_timer = entity->fall_timer;
    snapshot.stun_timer = entity->stun_timer;
    snapshot.projectile_contact_timer = entity->projectile_contact_timer;
    snapshot.thrown_by_id = FindExistingEntityLinkId(state, entity->thrown_by);
    snapshot.movement_flags = entity->movement_flags;
    snapshot.projectile_contact_damage_amount =
        static_cast<std::uint16_t>(std::min<unsigned int>(
            entity->projectile_contact_damage_amount,
            UINT16_MAX
        ));
    snapshot.jump_hold_gravity_frames_remaining =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->jump_hold_gravity_frames_remaining,
            UINT16_MAX
        ));
    snapshot.jump_delay_frame_count =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->jump_delay_frame_count,
            UINT16_MAX
        ));
    snapshot.climb_detach_cooldown =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->climb_detach_cooldown,
            UINT16_MAX
        ));
    snapshot.hang_count =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->hang_count,
            UINT16_MAX
        ));
    snapshot.holding_timer =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->holding_timer,
            UINT16_MAX
        ));
    snapshot.bomb_throw_delay_countdown =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->bomb_throw_delay_countdown,
            UINT16_MAX
        ));
    snapshot.rope_throw_delay_countdown =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->rope_throw_delay_countdown,
            UINT16_MAX
        ));
    snapshot.attack_delay_countdown =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->attack_delay_countdown,
            UINT16_MAX
        ));
    snapshot.equip_delay_countdown =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->equip_delay_countdown,
            UINT16_MAX
        ));
    snapshot.thrown_immunity_timer =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(
            entity->thrown_immunity_timer,
            UINT16_MAX
        ));
    snapshot.animation_id = entity->frame_data_animator.animation_id;
    snapshot.animation_time = entity->frame_data_animator.current_time;
    snapshot.animation_speed = entity->frame_data_animator.speed;
    snapshot.animation_frame =
        static_cast<std::uint16_t>(std::min<std::size_t>(
            entity->frame_data_animator.current_frame,
            UINT16_MAX
        ));
    snapshot.animate = entity->frame_data_animator.animate ? 1 : 0;
    snapshot.animation_loop = entity->frame_data_animator.loop ? 1 : 0;
    snapshot.animation_finished = entity->frame_data_animator.finished ? 1 : 0;
    snapshot.facing = entity->facing == LeftOrRight::Right ? 1 : 0;
    snapshot.condition = static_cast<std::uint8_t>(entity->condition);
    snapshot.grounded = entity->grounded ? 1 : 0;
    snapshot.has_physics = entity->has_physics ? 1 : 0;
    snapshot.can_collide = entity->can_collide ? 1 : 0;
    snapshot.can_apply_projectile_contact = entity->can_apply_projectile_contact ? 1 : 0;
    snapshot.projectile_contact_damage_type =
        static_cast<std::uint8_t>(entity->projectile_contact_damage_type);
    snapshot.hang_side = EncodeHangSide(entity->hang_side);
    snapshot.input_flags = BuildInputFlags(slot.inputs);
    return snapshot;
}

PlayerSnapshotsPacket MakePlayerSnapshots(
    State& state,
    NetTransportRuntime& transport,
    bool local_only,
    std::size_t start_slot_index,
    std::size_t* next_slot_index
) {
    PlayerSnapshotsPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sequence = transport.next_snapshot_sequence++;
    *next_slot_index = state.players.slots.size();
    for (std::size_t slot_index = start_slot_index;
         slot_index < state.players.slots.size();
         ++slot_index) {
        if (packet.snapshot_count >= packet.snapshots.size()) {
            *next_slot_index = slot_index;
            break;
        }
        const PlayerSlot& slot = state.players.slots[slot_index];
        if (!slot.connected) {
            continue;
        }
        if (local_only && slot.connection_kind != PlayerConnectionKind::Local) {
            continue;
        }
        const std::optional<PlayerSnapshotEntry> snapshot = MakeSnapshotForSlot(state, slot);
        if (!snapshot.has_value()) {
            continue;
        }
        packet.snapshots[packet.snapshot_count++] = *snapshot;
    }
    return packet;
}

void CopySnapshotToTarget(NetRemotePlayerTarget& target, const PlayerSnapshotEntry& snapshot) {
    target.pos_x = snapshot.pos_x;
    target.pos_y = snapshot.pos_y;
    target.vel_x = snapshot.vel_x;
    target.vel_y = snapshot.vel_y;
    target.acc_x = snapshot.acc_x;
    target.acc_y = snapshot.acc_y;
    target.size_x = snapshot.size_x;
    target.size_y = snapshot.size_y;
    target.rotation = snapshot.rotation;
    target.health = snapshot.health;
    target.coyote_time = snapshot.coyote_time;
    target.fall_timer = snapshot.fall_timer;
    target.stun_timer = snapshot.stun_timer;
    target.projectile_contact_timer = snapshot.projectile_contact_timer;
    target.thrown_by_id = snapshot.thrown_by_id;
    target.movement_flags = snapshot.movement_flags;
    target.projectile_contact_damage_amount =
        snapshot.projectile_contact_damage_amount;
    target.jump_hold_gravity_frames_remaining =
        snapshot.jump_hold_gravity_frames_remaining;
    target.jump_delay_frame_count = snapshot.jump_delay_frame_count;
    target.climb_detach_cooldown = snapshot.climb_detach_cooldown;
    target.hang_count = snapshot.hang_count;
    target.holding_timer = snapshot.holding_timer;
    target.bomb_throw_delay_countdown = snapshot.bomb_throw_delay_countdown;
    target.rope_throw_delay_countdown = snapshot.rope_throw_delay_countdown;
    target.attack_delay_countdown = snapshot.attack_delay_countdown;
    target.equip_delay_countdown = snapshot.equip_delay_countdown;
    target.thrown_immunity_timer = snapshot.thrown_immunity_timer;
    target.animation_id = snapshot.animation_id;
    target.animation_time = snapshot.animation_time;
    target.animation_speed = snapshot.animation_speed;
    target.animation_frame = snapshot.animation_frame;
    target.animate = snapshot.animate;
    target.animation_loop = snapshot.animation_loop;
    target.animation_finished = snapshot.animation_finished;
    target.facing = snapshot.facing;
    target.condition = snapshot.condition;
    target.grounded = snapshot.grounded;
    target.has_physics = snapshot.has_physics;
    target.can_collide = snapshot.can_collide;
    target.can_apply_projectile_contact = snapshot.can_apply_projectile_contact;
    target.projectile_contact_damage_type =
        snapshot.projectile_contact_damage_type;
    target.hang_side = snapshot.hang_side;
}

bool ShouldApplyCanonicalTimersToLocalPrediction(
    const Entity& entity,
    const NetRemotePlayerTarget& target
) {
    if (target.condition != static_cast<std::uint8_t>(EntityCondition::Normal) ||
        entity.condition != EntityCondition::Normal) {
        return true;
    }
    if (entity.health != target.health) {
        return true;
    }
    if (entity.has_physics != (target.has_physics != 0) ||
        entity.can_collide != (target.can_collide != 0)) {
        return true;
    }
    return false;
}

NetRemotePlayerTarget& EnsureRemotePlayerTarget(
    NetTransportRuntime& transport,
    const PlayerSnapshotEntry& snapshot,
    const Vec2& current_pos,
    std::uint32_t sequence,
    std::uint64_t frame
) {
    for (NetRemotePlayerTarget& target : transport.remote_player_targets) {
        if (target.player_id == snapshot.player_id) {
            if (sequence <= target.sequence) {
                return target;
            }
            target.start_pos_x = current_pos.x;
            target.start_pos_y = current_pos.y;
            CopySnapshotToTarget(target, snapshot);
            target.sequence = sequence;
            target.interpolation_start_frame = frame;
            target.last_received_frame = frame;
            return target;
        }
    }

    NetRemotePlayerTarget target{
        .player_id = snapshot.player_id,
        .start_pos_x = current_pos.x,
        .start_pos_y = current_pos.y,
        .sequence = sequence,
        .interpolation_start_frame = frame,
        .last_received_frame = frame,
    };
    CopySnapshotToTarget(target, snapshot);
    transport.remote_player_targets.push_back(target);
    return transport.remote_player_targets.back();
}

} // namespace

void EnsureSpawnedPlayer(
    State& state,
    PlayerId player_id,
    bool local,
    bool primary,
    const Vec2& pos,
    const Graphics& graphics
) {
    const bool is_peer_local_player =
        state.net_session.role == NetRole::Peer &&
        player_id == state.net_session.local_player_id;
    const bool effective_local = local || is_peer_local_player;
    const bool effective_primary = primary || is_peer_local_player;

    PlayerSlot& slot = effective_local
        ? state.players.EnsureLocalPlayer(
              player_id,
              "Player " + std::to_string(player_id),
              effective_primary
          )
        : state.players.EnsureRemotePlayer(player_id, "Remote " + std::to_string(player_id));

    if (slot.entity_vid.has_value()) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(*slot.entity_vid)) {
            if (entity->active) {
                entity->pos = pos;
                state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), entity->vid);
                if (effective_local && effective_primary) {
                    state.controlled_entity_vid = entity->vid;
                }
                return;
            }
        }
        slot.entity_vid.reset();
    }

    const std::optional<VID> vid = SpawnPlayerForPlayerId(state, player_id, pos);
    if (vid.has_value()) {
        state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), *vid);
        state.UpdateSidForEntity(vid->id, graphics);
        if (effective_local && effective_primary) {
            state.controlled_entity_vid = *vid;
        }
    }
}

void SendSnapshotsToEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    const bool coordinator_to_peer = state.net_session.role == NetRole::Coordinator;
    std::size_t cursor = 0;
    while (cursor < state.players.slots.size()) {
        std::size_t next_cursor = state.players.slots.size();
        const PlayerSnapshotsPacket snapshots = MakePlayerSnapshots(
            state,
            transport,
            !coordinator_to_peer,
            cursor,
            &next_cursor
        );
        if (snapshots.snapshot_count > 0) {
            SendEncodedPacket(transport, endpoint, EncodePlayerSnapshots(snapshots));
        }
        if (next_cursor <= cursor) {
            break;
        }
        cursor = next_cursor;
    }
}

void SendSnapshotsToAllRemotes(State& state, NetTransportRuntime& transport) {
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendSnapshotsToEndpoint(state, transport, remote.endpoint);
    }
}

void RelaySnapshotsToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const PlayerSnapshotsPacket& snapshots
) {
    const EncodedNetPacket encoded = EncodePlayerSnapshots(snapshots);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, source_endpoint)) {
            continue;
        }
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

void ApplyPlayerSnapshots(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const PlayerSnapshotsPacket& packet
) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    for (std::uint32_t i = 0; i < packet.snapshot_count; ++i) {
        const PlayerSnapshotEntry& snapshot = packet.snapshots[i];
        if (snapshot.player_id == kInvalidPlayerId) {
            continue;
        }
        const PlayerSlot* const existing_slot = state.players.Find(snapshot.player_id);
        const bool local_slot =
            existing_slot != nullptr &&
            existing_slot->connection_kind == PlayerConnectionKind::Local;
        const bool local_prediction_repair =
            state.net_session.role == NetRole::Peer && local_slot;
        if (local_slot && !local_prediction_repair) {
            continue;
        }

        Vec2 current_pos = Vec2::New(snapshot.pos_x, snapshot.pos_y);
        bool needs_spawn = true;
        if (PlayerSlot* const slot = state.players.Find(snapshot.player_id);
            slot != nullptr && slot->entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*slot->entity_vid)) {
                if (entity->active) {
                    current_pos = entity->pos;
                    needs_spawn = false;
                }
            }
        }
        if (needs_spawn) {
            EnsureSpawnedPlayer(
                state,
                snapshot.player_id,
                false,
                false,
                Vec2::New(snapshot.pos_x, snapshot.pos_y),
                graphics
            );
        }
        if (PlayerSlot* const slot = state.players.Find(snapshot.player_id);
            slot != nullptr && slot->connection_kind == PlayerConnectionKind::Remote) {
            ApplyInputFlags(*slot, snapshot.input_flags);
        }
        (void)EnsureRemotePlayerTarget(transport, snapshot, current_pos, packet.sequence, state.frame);
    }
}

void StepRemotePlayerInterpolation(
    State& state,
    NetTransportRuntime& transport,
    const Graphics& graphics
) {
    const float strength = std::clamp(transport.remote_interpolation_strength, 0.01F, 1.0F);
    const float delay_frames = static_cast<float>(transport.remote_interpolation_delay_frames);
    const float snap_distance = std::max(0.0F, transport.remote_snap_distance);
    const float snap_distance_sq = snap_distance * snap_distance;
    for (const NetRemotePlayerTarget& target : transport.remote_player_targets) {
        PlayerSlot* const slot = state.players.Find(target.player_id);
        if (slot == nullptr || !slot->entity_vid.has_value()) {
            continue;
        }

        const bool local_prediction_repair =
            state.net_session.role == NetRole::Peer &&
            slot->connection_kind == PlayerConnectionKind::Local;
        if (slot->connection_kind == PlayerConnectionKind::Local && !local_prediction_repair) {
            continue;
        }

        Entity* const entity = state.entity_manager.GetEntityMut(*slot->entity_vid);
        if (entity == nullptr || !entity->active) {
            continue;
        }

        const bool peer_snapshot_on_coordinator =
            state.net_session.role == NetRole::Coordinator &&
            slot->connection_kind == PlayerConnectionKind::Remote;
        if (peer_snapshot_on_coordinator) {
            // Coordinator receives peer snapshots as input packets. Durable
            // body state is produced by coordinator simulation, not by peer
            // position targets.
            continue;
        }

        const bool attachment_driven =
            entity->held_by_vid.has_value() || entity->attachment_mode != AttachmentMode::None;
        const Vec2 final_target_pos = Vec2::New(target.pos_x, target.pos_y);
        if (!attachment_driven) {
            const Vec2 final_delta = final_target_pos - entity->pos;
            const float final_distance_sq =
                final_delta.x * final_delta.x + final_delta.y * final_delta.y;
            Vec2 display_target_pos = final_target_pos;
            if (final_distance_sq <= snap_distance_sq && delay_frames > 0.0F) {
                const float age_frames = static_cast<float>(state.frame - target.interpolation_start_frame);
                const float t = std::clamp(age_frames / delay_frames, 0.0F, 1.0F);
                const Vec2 start_pos = Vec2::New(target.start_pos_x, target.start_pos_y);
                display_target_pos = start_pos + (final_target_pos - start_pos) * t;
            }

            const Vec2 delta = display_target_pos - entity->pos;
            if (final_distance_sq > snap_distance_sq) {
                entity->pos = final_target_pos;
            } else {
                entity->pos += delta * strength;
            }
            entity->vel = Vec2::New(target.vel_x, target.vel_y);
        }
        entity->acc = Vec2::New(target.acc_x, target.acc_y);
        entity->size = Vec2::New(target.size_x, target.size_y);
        entity->rotation = target.rotation;
        entity->health = target.health;
        entity->coyote_time = target.coyote_time;
        const bool apply_canonical_timers =
            !local_prediction_repair ||
            ShouldApplyCanonicalTimersToLocalPrediction(*entity, target);
        if (apply_canonical_timers) {
            entity->fall_timer = target.fall_timer;
        }
        entity->stun_timer = target.stun_timer;
        entity->projectile_contact_timer = target.projectile_contact_timer;
        entity->thrown_by = target.thrown_by_id != kInvalidNetEntityId
            ? state.net_session.FindLocalVid(target.thrown_by_id)
            : std::nullopt;
        entity->movement_flags = target.movement_flags;
        entity->projectile_contact_damage_amount =
            target.projectile_contact_damage_amount;
        entity->jump_hold_gravity_frames_remaining =
            target.jump_hold_gravity_frames_remaining;
        entity->jump_delay_frame_count = target.jump_delay_frame_count;
        entity->climb_detach_cooldown = target.climb_detach_cooldown;
        entity->hang_count = target.hang_count;
        entity->holding_timer = target.holding_timer;
        entity->bomb_throw_delay_countdown = target.bomb_throw_delay_countdown;
        entity->rope_throw_delay_countdown = target.rope_throw_delay_countdown;
        entity->attack_delay_countdown = target.attack_delay_countdown;
        entity->equip_delay_countdown = target.equip_delay_countdown;
        entity->thrown_immunity_timer = target.thrown_immunity_timer;
        entity->condition = static_cast<EntityCondition>(target.condition);
        entity->grounded = target.grounded != 0;
        entity->has_physics = target.has_physics != 0;
        entity->can_collide = target.can_collide != 0;
        entity->can_apply_projectile_contact = target.can_apply_projectile_contact != 0;
        entity->projectile_contact_damage_type =
            static_cast<DamageType>(target.projectile_contact_damage_type);
        entity->hang_side = DecodeHangSide(target.hang_side);
        entity->facing = target.facing != 0 ? LeftOrRight::Right : LeftOrRight::Left;
        if (target.animation_id != kInvalidFrameDataId) {
            FrameDataAnimator& animator = entity->frame_data_animator;
            if (animator.animation_id != target.animation_id) {
                animator.SetAnimation(target.animation_id);
            }
            animator.current_frame = target.animation_frame;
            animator.current_time = target.animation_time;
            animator.speed = target.animation_speed;
            animator.animate = target.animate != 0;
            animator.loop = target.animation_loop != 0;
            animator.finished = target.animation_finished != 0;
        }
        state.UpdateSidForEntity(entity->vid.id, graphics);
    }
}

void SyncNetworkAttachmentsAfterRemoteMovement(State& state, const Graphics& graphics) {
    constexpr int kAttachmentSyncPasses = 8;
    for (int pass = 0; pass < kAttachmentSyncPasses; ++pass) {
        for (std::size_t entity_idx = 0; entity_idx < state.entity_manager.entities.size(); ++entity_idx) {
            entities::common::SyncEntityAttachments(entity_idx, state, graphics);
        }
    }
}

bool ShouldSendSnapshots(const State& state, const NetTransportRuntime& transport) {
    const std::uint32_t interval = std::max<std::uint32_t>(transport.snapshot_send_interval_frames, 1);
    return (state.frame % interval) == 0;
}

} // namespace splonks::network

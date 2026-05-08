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

std::optional<PlayerSnapshotEntry> MakeSnapshotForSlot(const State& state, const PlayerSlot& slot) {
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
    snapshot.health = entity->health;
    snapshot.coyote_time = entity->coyote_time;
    snapshot.fall_timer = entity->fall_timer;
    snapshot.stun_timer = entity->stun_timer;
    snapshot.projectile_contact_timer = entity->projectile_contact_timer;
    snapshot.facing = entity->facing == LeftOrRight::Right ? 1 : 0;
    snapshot.condition = static_cast<std::uint8_t>(entity->condition);
    snapshot.grounded = entity->grounded ? 1 : 0;
    snapshot.has_physics = entity->has_physics ? 1 : 0;
    snapshot.can_collide = entity->can_collide ? 1 : 0;
    snapshot.input_flags = BuildInputFlags(slot.inputs);
    return snapshot;
}

PlayerSnapshotsPacket MakePlayerSnapshots(
    State& state,
    NetTransportRuntime& transport,
    bool local_only
) {
    PlayerSnapshotsPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sequence = transport.next_snapshot_sequence++;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected) {
            continue;
        }
        if (local_only && slot.connection_kind != PlayerConnectionKind::Local) {
            continue;
        }
        if (packet.snapshot_count >= packet.snapshots.size()) {
            break;
        }
        const std::optional<PlayerSnapshotEntry> snapshot = MakeSnapshotForSlot(state, slot);
        if (!snapshot.has_value()) {
            continue;
        }
        packet.snapshots[packet.snapshot_count++] = *snapshot;
    }
    return packet;
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
            target.pos_x = snapshot.pos_x;
            target.pos_y = snapshot.pos_y;
            target.vel_x = snapshot.vel_x;
            target.vel_y = snapshot.vel_y;
            target.health = snapshot.health;
            target.coyote_time = snapshot.coyote_time;
            target.fall_timer = snapshot.fall_timer;
            target.stun_timer = snapshot.stun_timer;
            target.projectile_contact_timer = snapshot.projectile_contact_timer;
            target.facing = snapshot.facing;
            target.condition = snapshot.condition;
            target.grounded = snapshot.grounded;
            target.has_physics = snapshot.has_physics;
            target.can_collide = snapshot.can_collide;
            target.sequence = sequence;
            target.interpolation_start_frame = frame;
            target.last_received_frame = frame;
            return target;
        }
    }

    transport.remote_player_targets.push_back(NetRemotePlayerTarget{
        .player_id = snapshot.player_id,
        .start_pos_x = current_pos.x,
        .start_pos_y = current_pos.y,
        .pos_x = snapshot.pos_x,
        .pos_y = snapshot.pos_y,
        .vel_x = snapshot.vel_x,
        .vel_y = snapshot.vel_y,
        .health = snapshot.health,
        .coyote_time = snapshot.coyote_time,
        .fall_timer = snapshot.fall_timer,
        .stun_timer = snapshot.stun_timer,
        .projectile_contact_timer = snapshot.projectile_contact_timer,
        .facing = snapshot.facing,
        .condition = snapshot.condition,
        .grounded = snapshot.grounded,
        .has_physics = snapshot.has_physics,
        .can_collide = snapshot.can_collide,
        .sequence = sequence,
        .interpolation_start_frame = frame,
        .last_received_frame = frame,
    });
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
    PlayerSlot& slot = local
        ? state.players.EnsureLocalPlayer(player_id, "Player " + std::to_string(player_id), primary)
        : state.players.EnsureRemotePlayer(player_id, "Remote " + std::to_string(player_id));

    if (slot.entity_vid.has_value()) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(*slot.entity_vid)) {
            if (entity->active) {
                entity->pos = pos;
                state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), entity->vid);
                if (local && primary) {
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
        if (local && primary) {
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
    const PlayerSnapshotsPacket snapshots = MakePlayerSnapshots(
        state,
        transport,
        !coordinator_to_peer
    );
    if (snapshots.snapshot_count == 0) {
        return;
    }
    SendEncodedPacket(transport, endpoint, EncodePlayerSnapshots(snapshots));
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

        const bool target_is_world_anchored = target.grounded != 0;
        const bool locally_predicted_thrown =
            entity->projectile_contact_timer > 0 &&
            !entity->held_by_vid.has_value() &&
            entity->attachment_mode == AttachmentMode::None &&
            !target_is_world_anchored;

        const bool attachment_driven =
            entity->held_by_vid.has_value() || entity->attachment_mode != AttachmentMode::None;
        const Vec2 final_target_pos = Vec2::New(target.pos_x, target.pos_y);
        if (!attachment_driven && !locally_predicted_thrown) {
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
        entity->health = target.health;
        entity->coyote_time = target.coyote_time;
        entity->fall_timer = target.fall_timer;
        entity->stun_timer = target.stun_timer;
        entity->projectile_contact_timer = target.projectile_contact_timer;
        entity->condition = static_cast<EntityCondition>(target.condition);
        entity->grounded = target.grounded != 0;
        entity->has_physics = target.has_physics != 0;
        entity->can_collide = target.can_collide != 0;
        entity->facing = target.facing != 0 ? LeftOrRight::Right : LeftOrRight::Left;
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

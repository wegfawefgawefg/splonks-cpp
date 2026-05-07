#include "network/net_lobby_internal.hpp"

#include "entities/common/common.hpp"
#include "network/net_protocol.hpp"
#include "stage_spawning.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace splonks::network {

namespace {

constexpr std::uint16_t kPlayerSnapshotFlagClimbing = 1U << 0U;
constexpr std::uint16_t kPlayerSnapshotFlagHanging = 1U << 1U;
constexpr std::uint16_t kPlayerSnapshotFlagHangRight = 1U << 2U;
constexpr std::uint16_t kPlayerSnapshotInputLeft = 1U << 0U;
constexpr std::uint16_t kPlayerSnapshotInputRight = 1U << 1U;
constexpr std::uint16_t kPlayerSnapshotInputUp = 1U << 2U;
constexpr std::uint16_t kPlayerSnapshotInputDown = 1U << 3U;
constexpr std::uint16_t kPlayerSnapshotInputAttack = 1U << 4U;
constexpr std::uint16_t kPlayerSnapshotInputUseBack = 1U << 5U;

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
    if (inputs.attack.down) {
        flags |= kPlayerSnapshotInputAttack;
    }
    if (inputs.use_button.down) {
        flags |= kPlayerSnapshotInputUseBack;
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
    slot.inputs.attack = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputAttack) != 0,
        slot.inputs.attack
    );
    slot.inputs.use_button = BuildButtonStateFromNetwork(
        (flags & kPlayerSnapshotInputUseBack) != 0,
        slot.inputs.use_button
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
    snapshot.facing = entity->facing == LeftOrRight::Right ? 1 : 0;
    snapshot.condition = static_cast<std::uint8_t>(entity->condition);
    snapshot.grounded = entity->grounded ? 1 : 0;
    snapshot.animate = entity->frame_data_animator.animate ? 1 : 0;
    snapshot.input_flags = BuildInputFlags(slot.inputs);
    snapshot.animation_id = entity->frame_data_animator.animation_id;
    snapshot.animation_frame = static_cast<std::uint16_t>(std::min<std::size_t>(
        entity->frame_data_animator.current_frame,
        std::numeric_limits<std::uint16_t>::max()
    ));
    if (entity->IsClimbing()) {
        snapshot.animation_flags |= kPlayerSnapshotFlagClimbing;
    }
    if (entity->hang_side.has_value()) {
        snapshot.animation_flags |= kPlayerSnapshotFlagHanging;
        if (*entity->hang_side == LeftOrRight::Right) {
            snapshot.animation_flags |= kPlayerSnapshotFlagHangRight;
        }
    }
    snapshot.animation_time = entity->frame_data_animator.current_time;
    snapshot.animation_speed = entity->frame_data_animator.speed;
    return snapshot;
}

PlayerSnapshotsPacket MakeLocalPlayerSnapshots(State& state, NetTransportRuntime& transport) {
    PlayerSnapshotsPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sequence = transport.next_snapshot_sequence++;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connection_kind != PlayerConnectionKind::Local || !slot.connected) {
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
            target.facing = snapshot.facing;
            target.condition = snapshot.condition;
            target.grounded = snapshot.grounded;
            target.animate = snapshot.animate;
            target.animation_id = snapshot.animation_id;
            target.animation_frame = snapshot.animation_frame;
            target.animation_flags = snapshot.animation_flags;
            target.animation_time = snapshot.animation_time;
            target.animation_speed = snapshot.animation_speed;
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
        .facing = snapshot.facing,
        .condition = snapshot.condition,
        .grounded = snapshot.grounded,
        .animate = snapshot.animate,
        .animation_id = snapshot.animation_id,
        .animation_frame = snapshot.animation_frame,
        .animation_flags = snapshot.animation_flags,
        .animation_time = snapshot.animation_time,
        .animation_speed = snapshot.animation_speed,
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
    const PlayerSnapshotsPacket snapshots = MakeLocalPlayerSnapshots(state, transport);
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
        if (existing_slot != nullptr &&
            existing_slot->connection_kind == PlayerConnectionKind::Local) {
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
        if (slot == nullptr ||
            slot->connection_kind == PlayerConnectionKind::Local ||
            !slot->entity_vid.has_value()) {
            continue;
        }

        Entity* const entity = state.entity_manager.GetEntityMut(*slot->entity_vid);
        if (entity == nullptr || !entity->active) {
            continue;
        }

        const bool target_is_world_anchored =
            target.grounded != 0 ||
            (target.animation_flags & (kPlayerSnapshotFlagClimbing | kPlayerSnapshotFlagHanging)) != 0;
        const bool locally_predicted_thrown =
            entity->projectile_contact_timer > 0 &&
            !entity->held_by_vid.has_value() &&
            entity->attachment_mode == AttachmentMode::None &&
            !target_is_world_anchored;

        const bool attachment_driven =
            entity->held_by_vid.has_value() || entity->attachment_mode != AttachmentMode::None;
        const Vec2 final_target_pos = Vec2::New(target.pos_x, target.pos_y);
        if (!attachment_driven && !locally_predicted_thrown) {
            Vec2 display_target_pos = final_target_pos;
            if (delay_frames > 0.0F) {
                const float age_frames = static_cast<float>(state.frame - target.interpolation_start_frame);
                const float t = std::clamp(age_frames / delay_frames, 0.0F, 1.0F);
                const Vec2 start_pos = Vec2::New(target.start_pos_x, target.start_pos_y);
                display_target_pos = start_pos + (final_target_pos - start_pos) * t;
            }

            const Vec2 delta = display_target_pos - entity->pos;
            const float distance_sq = delta.x * delta.x + delta.y * delta.y;
            if (distance_sq > snap_distance_sq) {
                entity->pos = final_target_pos;
            } else {
                entity->pos += delta * strength;
            }
            entity->vel = Vec2::New(target.vel_x, target.vel_y);
            entity->grounded = target.grounded != 0;
        }
        entity->facing = target.facing != 0 ? LeftOrRight::Right : LeftOrRight::Left;
        entity->condition = static_cast<EntityCondition>(target.condition);
        SetMovementFlag(
            *entity,
            EntityMovementFlag::Climbing,
            (target.animation_flags & kPlayerSnapshotFlagClimbing) != 0
        );
        if ((target.animation_flags & kPlayerSnapshotFlagHanging) != 0) {
            entity->hang_side = (target.animation_flags & kPlayerSnapshotFlagHangRight) != 0
                                    ? LeftOrRight::Right
                                    : LeftOrRight::Left;
        } else {
            entity->hang_side.reset();
        }
        if (target.animation_id != kInvalidFrameDataId) {
            FrameDataAnimator& animator = entity->frame_data_animator;
            if (animator.animation_id != target.animation_id) {
                animator.SetAnimation(target.animation_id);
            }
            animator.current_frame = target.animation_frame;
            animator.current_time = target.animation_time;
            animator.speed = target.animation_speed;
            animator.animate = target.animate != 0;
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

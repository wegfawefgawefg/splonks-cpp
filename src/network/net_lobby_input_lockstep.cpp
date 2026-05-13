#include "network/net_lobby_internal.hpp"

#include "inputs.hpp"
#include "simulation_snapshot.hpp"
#include "state_fingerprint.hpp"
#include "state.hpp"
#include "step.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

namespace splonks::network {

namespace {

constexpr LockstepFrame kInputHistoryFrames = 12;
constexpr std::size_t kMaxPendingRemoteHashes = 128;

struct RollbackPresentationSnapshot {
    ParticleSystem particles;
    AudioEmitterManager audio_emitters;
    StageLighting stage_lighting;
    Vec2 audio_listener_world_pos = Vec2::New(0.0F, 0.0F);
    std::optional<Vec2> gameplay_camera_anchor_world_pos;
    Vec2 play_cam_pos = Vec2::New(0.0F, 0.0F);
};

void PruneRollbackSnapshots(State& state);
void RequestRollbackFromFrame(State& state, LockstepFrame frame);
const GameplaySnapshot* FindRollbackSnapshot(const State& state, LockstepFrame frame);

float ElapsedMs(std::chrono::steady_clock::time_point start) {
    const std::chrono::duration<float, std::milli> elapsed =
        std::chrono::steady_clock::now() - start;
    return elapsed.count();
}

std::uint32_t SuggestedLockstepDelayFrames(float ping_ms, float jitter_ms) {
    constexpr float kNetworkFrameMs = 1000.0F / 60.0F;
    constexpr float kSafetyFrames = 1.0F;
    const float one_way_ms = std::max(0.0F, ping_ms) * 0.5F;
    const float jitter_margin_ms = std::max(2.0F, std::max(0.0F, jitter_ms) * 2.0F);
    const float frames = std::ceil((one_way_ms + jitter_margin_ms) / kNetworkFrameMs + kSafetyFrames);
    return ClampLockstepInputDelayFrames(static_cast<std::uint32_t>(std::max(0.0F, frames)));
}

std::size_t ApproxRollbackBufferBytes(const State& state) {
    std::size_t bytes = 0;
    for (const LockstepRollbackSnapshot& entry : state.net_session.lockstep_rollback_snapshots) {
        if (entry.snapshot) {
            bytes += sizeof(GameplaySnapshot);
            bytes += entry.snapshot->ents.ents.capacity() * sizeof(Ent);
            bytes += entry.snapshot->stage.tiles.capacity() * sizeof(std::vector<Tile>);
            bytes += entry.snapshot->particles.sprite_particles.capacity() *
                sizeof(SpriteParticle);
        }
    }
    return bytes;
}

LockstepHashRecord* FindLocalHashRecord(State& state, LockstepFrame frame) {
    for (LockstepHashRecord& record : state.net_session.lockstep_hash_history) {
        if (record.frame == frame) {
            return &record;
        }
    }
    return nullptr;
}

const LockstepHashRecord* FindLocalHashRecord(const State& state, LockstepFrame frame) {
    for (const LockstepHashRecord& record : state.net_session.lockstep_hash_history) {
        if (record.frame == frame) {
            return &record;
        }
    }
    return nullptr;
}

bool RequestSnapshotCatchupFromStageStart(State& state) {
    if (FindRollbackSnapshot(state, 0) == nullptr) {
        state.net_session.lockstep_last_desync_recovery_mode =
            LockstepDesyncRecoveryMode::FatalDesync;
        return false;
    }
    state.net_session.lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::SnapshotCatchup;
    RequestRollbackFromFrame(state, 0);
    return true;
}

void PruneLockstepHashHistory(State& state) {
    const LockstepFrame next_frame = state.net_session.lockstep_next_frame_to_step;
    const LockstepFrame keep_frames = std::max<LockstepFrame>(
        static_cast<LockstepFrame>(state.net_session.lockstep_max_rollback_frames) + 8,
        static_cast<LockstepFrame>(state.net_session.lockstep_hash_send_interval_frames) * 4 + 8
    );
    const LockstepFrame first_kept = next_frame > keep_frames ? next_frame - keep_frames : 0;
    auto& history = state.net_session.lockstep_hash_history;
    history.erase(
        std::remove_if(
            history.begin(),
            history.end(),
            [first_kept](const LockstepHashRecord& record) {
                return record.frame < first_kept;
            }
        ),
        history.end()
    );

    auto& pending = state.net_session.lockstep_pending_remote_hashes;
    pending.erase(
        std::remove_if(
            pending.begin(),
            pending.end(),
            [first_kept](const LockstepRemoteHashRecord& record) {
                return record.frame < first_kept;
            }
        ),
        pending.end()
    );

    auto& remote_history = state.net_session.lockstep_remote_hash_history;
    remote_history.erase(
        std::remove_if(
            remote_history.begin(),
            remote_history.end(),
            [first_kept](const LockstepRemoteHashRecord& record) {
                return record.frame < first_kept;
            }
        ),
        remote_history.end()
    );
}

void RecordRemoteHashSample(State& state, const LockstepRemoteHashRecord& remote) {
    auto& history = state.net_session.lockstep_remote_hash_history;
    const auto existing = std::find_if(
        history.begin(),
        history.end(),
        [&remote](const LockstepRemoteHashRecord& record) {
            return record.peer_id == remote.peer_id && record.frame == remote.frame;
        }
    );
    if (existing != history.end()) {
        *existing = remote;
        return;
    }
    history.push_back(remote);
}

std::optional<LockstepHashRecord> FindLastMatchingHashWithPeer(
    const State& state,
    PlayerId peer_id,
    LockstepFrame before_frame
) {
    std::optional<LockstepHashRecord> best;
    for (const LockstepRemoteHashRecord& remote : state.net_session.lockstep_remote_hash_history) {
        if (remote.peer_id != peer_id || remote.frame >= before_frame) {
            continue;
        }
        const LockstepHashRecord* const local = FindLocalHashRecord(state, remote.frame);
        if (local == nullptr || local->hash != remote.hash) {
            continue;
        }
        if (!best.has_value() || remote.frame > best->frame) {
            best = LockstepHashRecord{
                .frame = remote.frame,
                .hash = local->hash,
            };
        }
    }
    return best;
}

void CompareLockstepHash(State& state, const LockstepRemoteHashRecord& remote) {
    const LockstepHashRecord* const local = FindLocalHashRecord(state, remote.frame);
    if (local == nullptr) {
        return;
    }
    RecordRemoteHashSample(state, remote);

    if (local->hash == remote.hash) {
        if (!state.net_session.lockstep_has_confirmed_hash ||
            remote.frame >= state.net_session.lockstep_last_confirmed_hash_frame) {
            state.net_session.lockstep_last_confirmed_hash_frame = remote.frame;
            state.net_session.lockstep_last_confirmed_hash = local->hash;
            state.net_session.lockstep_has_confirmed_hash = true;
        }
        if (state.net_session.lockstep_last_desync_recovery_mode ==
            LockstepDesyncRecoveryMode::PendingRollback) {
            state.net_session.lockstep_last_desync_recovery_mode =
                LockstepDesyncRecoveryMode::RollbackRepaired;
        }
        return;
    }

    state.net_session.lockstep_hash_mismatch_count += 1;
    state.net_session.lockstep_last_mismatch_peer_id = remote.peer_id;
    state.net_session.lockstep_last_mismatch_frame = remote.frame;
    state.net_session.lockstep_last_mismatch_local_hash = local->hash;
    state.net_session.lockstep_last_mismatch_remote_hash = remote.hash;

    const std::optional<LockstepHashRecord> last_peer_match =
        FindLastMatchingHashWithPeer(state, remote.peer_id, remote.frame);
    if (!last_peer_match.has_value()) {
        (void)RequestSnapshotCatchupFromStageStart(state);
        return;
    }

    const LockstepFrame rollback_frame =
        remote.frame > last_peer_match->frame
            ? last_peer_match->frame + 1
            : remote.frame;
    if (FindRollbackSnapshot(state, rollback_frame) != nullptr) {
        state.net_session.lockstep_last_desync_recovery_mode =
            LockstepDesyncRecoveryMode::PendingRollback;
        RequestRollbackFromFrame(state, rollback_frame);
    } else {
        (void)RequestSnapshotCatchupFromStageStart(state);
    }
}

void ValidateRemoteHashesAfterReplay(
    State& state,
    LockstepFrame first_frame,
    LockstepFrame end_frame
) {
    bool checked_any = false;
    for (const LockstepRemoteHashRecord& remote : state.net_session.lockstep_remote_hash_history) {
        if (remote.frame < first_frame || remote.frame >= end_frame) {
            continue;
        }
        const LockstepHashRecord* const local = FindLocalHashRecord(state, remote.frame);
        if (local == nullptr) {
            continue;
        }
        checked_any = true;
        if (local->hash != remote.hash) {
            state.net_session.lockstep_last_mismatch_peer_id = remote.peer_id;
            state.net_session.lockstep_last_mismatch_frame = remote.frame;
            state.net_session.lockstep_last_mismatch_local_hash = local->hash;
            state.net_session.lockstep_last_mismatch_remote_hash = remote.hash;
            if (first_frame == 0) {
                state.net_session.lockstep_last_desync_recovery_mode =
                    LockstepDesyncRecoveryMode::FatalDesync;
            } else {
                (void)RequestSnapshotCatchupFromStageStart(state);
            }
            return;
        }
        if (!state.net_session.lockstep_has_confirmed_hash ||
            remote.frame >= state.net_session.lockstep_last_confirmed_hash_frame) {
            state.net_session.lockstep_last_confirmed_hash_frame = remote.frame;
            state.net_session.lockstep_last_confirmed_hash = local->hash;
            state.net_session.lockstep_has_confirmed_hash = true;
        }
    }
    if (checked_any && (state.net_session.lockstep_last_desync_recovery_mode ==
            LockstepDesyncRecoveryMode::PendingRollback ||
        state.net_session.lockstep_last_desync_recovery_mode ==
            LockstepDesyncRecoveryMode::SnapshotCatchup)) {
        state.net_session.lockstep_last_desync_recovery_mode =
            LockstepDesyncRecoveryMode::RollbackRepaired;
    }
}

void ProcessPendingRemoteHashes(State& state) {
    auto& pending = state.net_session.lockstep_pending_remote_hashes;
    pending.erase(
        std::remove_if(
            pending.begin(),
            pending.end(),
            [&state](const LockstepRemoteHashRecord& remote) {
                if (FindLocalHashRecord(state, remote.frame) == nullptr) {
                    return false;
                }
                CompareLockstepHash(state, remote);
                return true;
            }
        ),
        pending.end()
    );
}

void StoreOrCompareRemoteHash(State& state, const LockstepRemoteHashRecord& remote) {
    if (FindLocalHashRecord(state, remote.frame) != nullptr) {
        CompareLockstepHash(state, remote);
        return;
    }

    auto& pending = state.net_session.lockstep_pending_remote_hashes;
    const auto existing = std::find_if(
        pending.begin(),
        pending.end(),
        [&remote](const LockstepRemoteHashRecord& record) {
            return record.peer_id == remote.peer_id && record.frame == remote.frame;
        }
    );
    if (existing != pending.end()) {
        *existing = remote;
        return;
    }
    pending.push_back(remote);
    if (pending.size() > kMaxPendingRemoteHashes) {
        pending.erase(pending.begin());
    }
}

void RecordCompletedLockstepHash(State& state, LockstepFrame frame) {
    if (!IsInputLockstepSession(state)) {
        return;
    }
    if (state.net_session.lockstep_has_recorded_hash &&
        state.net_session.lockstep_last_recorded_hash_frame == frame) {
        return;
    }

    const auto hash_start = std::chrono::steady_clock::now();
    const CanonicalStateFingerprint fingerprint = ComputeGameplayDeterminismFingerprint(state);
    state.performance_stats.lockstep_hash_ms += ElapsedMs(hash_start);

    LockstepHashRecord* const existing = FindLocalHashRecord(state, frame);
    if (existing != nullptr) {
        existing->hash = fingerprint.value;
    } else {
        state.net_session.lockstep_hash_history.push_back(LockstepHashRecord{
            .frame = frame,
            .hash = fingerprint.value,
        });
    }
    state.net_session.lockstep_last_recorded_hash_frame = frame;
    state.net_session.lockstep_has_recorded_hash = true;
    PruneLockstepHashHistory(state);
    ProcessPendingRemoteHashes(state);
}

void RecordPreviousCompletedLockstepHash(State& state) {
    if (state.net_session.lockstep_next_frame_to_step == 0) {
        return;
    }
    RecordCompletedLockstepHash(state, state.net_session.lockstep_next_frame_to_step - 1);
}

LockstepHashNetPacket MakeLockstepHashPacket(const State& state, const LockstepHashRecord& record) {
    LockstepHashNetPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    packet.frame = record.frame;
    packet.hash = record.hash;
    return packet;
}

void SendDueLockstepHash(State& state, NetTransportRuntime& transport) {
    if (!state.net_session.lockstep_has_recorded_hash) {
        return;
    }
    const LockstepFrame frame = state.net_session.lockstep_last_recorded_hash_frame;
    const LockstepFrame interval = std::max<LockstepFrame>(
        1,
        static_cast<LockstepFrame>(state.net_session.lockstep_hash_send_interval_frames)
    );
    if (state.net_session.lockstep_has_sent_hash &&
        frame < state.net_session.lockstep_last_sent_hash_frame + interval) {
        return;
    }

    const LockstepHashRecord* const record = FindLocalHashRecord(state, frame);
    if (record == nullptr) {
        return;
    }
    const EncodedNetPacket encoded = EncodeLockstepHash(MakeLockstepHashPacket(state, *record));
    if (state.net_session.role == NetRole::Host) {
        for (const NetRemoteEndpoint& remote : transport.remotes) {
            SendEncodedPacket(transport, remote.endpoint, encoded);
        }
    } else if (state.net_session.role == NetRole::Peer && !transport.join_request_pending) {
        SendEncodedPacket(transport, transport.host_endpoint, encoded);
    }
    state.net_session.lockstep_last_sent_hash_frame = frame;
    state.net_session.lockstep_has_sent_hash = true;
}

RollbackPresentationSnapshot CaptureRollbackPresentationState(
    const State& state,
    const Graphics& graphics
) {
    RollbackPresentationSnapshot snapshot;
    snapshot.particles = state.particles;
    snapshot.audio_emitters = state.audio_emitters;
    snapshot.stage_lighting = state.stage_lighting;
    snapshot.audio_listener_world_pos = state.audio_listener_world_pos;
    snapshot.gameplay_camera_anchor_world_pos = state.gameplay_camera_anchor_world_pos;
    snapshot.play_cam_pos = graphics.play_cam.pos;
    return snapshot;
}

void RestoreRollbackPresentationState(
    const RollbackPresentationSnapshot& snapshot,
    State& state,
    Graphics& graphics
) {
    state.particles = snapshot.particles;
    state.audio_emitters = snapshot.audio_emitters;
    state.stage_lighting = snapshot.stage_lighting;
    state.audio_listener_world_pos = snapshot.audio_listener_world_pos;
    state.gameplay_camera_anchor_world_pos = snapshot.gameplay_camera_anchor_world_pos;
    graphics.play_cam.pos = snapshot.play_cam_pos;

    // Corrected gameplay may have changed tiles/fluids while presentation state was
    // preserved from the pre-rollback present. Rebuild caches on demand.
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
}

std::vector<PlayerId> GetConnectedPlayerIds(const State& state) {
    std::vector<PlayerId> player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.player_id != kInvalidPlayerId && slot.ent_vid.has_value()) {
            player_ids.push_back(slot.player_id);
        }
    }
    std::sort(player_ids.begin(), player_ids.end());
    return player_ids;
}

std::vector<PlayerId> GetLocalPlayerIds(const State& state) {
    std::vector<PlayerId> player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local &&
            slot.player_id != kInvalidPlayerId && slot.ent_vid.has_value()) {
            player_ids.push_back(slot.player_id);
        }
    }
    std::sort(player_ids.begin(), player_ids.end());
    return player_ids;
}

bool CanStepWithoutRunningAheadOfRemoteInputs(
    const State& state,
    const std::vector<PlayerId>& required_players
) {
    const std::vector<PlayerId> local_player_ids = GetLocalPlayerIds(state);
    const LockstepFrame frame_to_step = state.net_session.lockstep_next_frame_to_step;
    const LockstepFrame input_delay =
        static_cast<LockstepFrame>(state.net_session.lockstep_input_delay_frames);

    for (PlayerId player_id : required_players) {
        if (std::find(local_player_ids.begin(), local_player_ids.end(), player_id) !=
            local_player_ids.end()) {
            continue;
        }

        const std::optional<LockstepFrame> latest =
            state.net_session.lockstep_input_buffer.LatestFrameForPlayer(
                player_id,
                !state.net_session.lockstep_rollback_enabled
            );
        if (!latest.has_value() || *latest < input_delay) {
            return false;
        }

        const LockstepFrame prediction_window = state.net_session.lockstep_rollback_enabled
            ? static_cast<LockstepFrame>(state.net_session.lockstep_max_rollback_frames)
            : 0;
        const LockstepFrame remote_advertised_step_frame = *latest - input_delay + prediction_window;
        if (frame_to_step > remote_advertised_step_frame) {
            return false;
        }
    }

    return true;
}

InputFrame GetCurrentLocalInputFrame(const State& state, const PlayerSlot& slot) {
    if (slot.primary_local) {
        return ToInputFrame(state.playing_input_snapshot);
    }
    return slot.input_frame;
}

void QueueLocalInputsThroughTargetFrame(State& state) {
    const LockstepFrame target_frame =
        state.net_session.lockstep_next_frame_to_step +
        static_cast<LockstepFrame>(state.net_session.lockstep_input_delay_frames);

    while (state.net_session.lockstep_next_local_input_frame <= target_frame) {
        const LockstepFrame frame = state.net_session.lockstep_next_local_input_frame;
        for (const PlayerSlot& slot : state.players.slots) {
            if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local ||
                slot.player_id == kInvalidPlayerId || !slot.ent_vid.has_value()) {
                continue;
            }
            LockstepInputRecord record;
            record.player_id = slot.player_id;
            record.frame = frame;
            record.sequence = state.net_session.lockstep_next_input_sequence;
            record.input = GetCurrentLocalInputFrame(state, slot);
            state.net_session.lockstep_input_buffer.Store(record);
        }
        state.net_session.lockstep_next_local_input_frame += 1;
        state.net_session.lockstep_next_input_sequence += 1;
    }
}

InputFrameRecordEntry MakeInputFrameRecordEntry(const LockstepInputRecord& record) {
    InputFrameRecordEntry entry;
    entry.player_id = record.player_id;
    entry.frame = record.frame;
    entry.sequence = record.sequence;
    entry.input_flags = PackInputFrame(record.input);
    entry.mouse_x = record.input.mouse_pos.x;
    entry.mouse_y = record.input.mouse_pos.y;
    return entry;
}

LockstepInputRecord MakeLockstepInputRecord(const InputFrameRecordEntry& entry) {
    LockstepInputRecord record;
    record.player_id = entry.player_id;
    record.frame = entry.frame;
    record.sequence = entry.sequence;
    record.input = UnpackInputFrame(entry.input_flags, UVec2::New(entry.mouse_x, entry.mouse_y));
    return record;
}

InputFrameRecordsPacket BuildLocalInputFramePacket(State& state) {
    InputFrameRecordsPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);

    const std::vector<PlayerId> local_player_ids = GetLocalPlayerIds(state);
    if (local_player_ids.empty() || state.net_session.lockstep_next_local_input_frame == 0) {
        return packet;
    }

    const LockstepFrame last_frame = state.net_session.lockstep_next_local_input_frame - 1;
    const LockstepFrame first_frame = last_frame > kInputHistoryFrames
        ? last_frame - kInputHistoryFrames
        : 0;
    std::vector<LockstepInputRecord> records;
    state.net_session.lockstep_input_buffer.CollectRecords(
        local_player_ids,
        first_frame,
        last_frame,
        records,
        packet.records.size()
    );
    packet.record_count = static_cast<std::uint32_t>(records.size());
    for (std::uint32_t i = 0; i < packet.record_count; ++i) {
        packet.records[i] = MakeInputFrameRecordEntry(records[i]);
    }
    return packet;
}

void SendInputFramePacketToEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    const InputFrameRecordsPacket packet = BuildLocalInputFramePacket(state);
    if (packet.record_count == 0) {
        return;
    }
    SendEncodedPacket(transport, endpoint, EncodeInputFrameRecords(packet));
}

void SendLocalInputFramePacket(State& state, NetTransportRuntime& transport) {
    if (state.net_session.role == NetRole::Host) {
        for (const NetRemoteEndpoint& remote : transport.remotes) {
            SendInputFramePacketToEndpoint(state, transport, remote.endpoint);
        }
        return;
    }
    if (state.net_session.role == NetRole::Peer && !transport.join_request_pending) {
        SendInputFramePacketToEndpoint(state, transport, transport.host_endpoint);
    }
}

LockstepSettingsPacket MakeLockstepSettingsPacket(
    const State& state,
    const PendingLockstepSettings& settings
) {
    LockstepSettingsPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    packet.sequence = settings.sequence;
    packet.apply_frame = settings.apply_frame;
    packet.input_delay_frames = settings.input_delay_frames;
    packet.max_rollback_frames = settings.max_rollback_frames;
    return packet;
}

void ClearExpiredBroadcastLockstepSettings(State& state) {
    if (!state.net_session.lockstep_broadcast_settings.has_value()) {
        return;
    }
    if (state.net_session.lockstep_next_frame_to_step <=
        state.net_session.lockstep_broadcast_settings_until_frame) {
        return;
    }
    state.net_session.lockstep_broadcast_settings = std::nullopt;
    state.net_session.lockstep_broadcast_settings_until_frame = 0;
}

void SendBroadcastLockstepSettings(State& state, NetTransportRuntime& transport) {
    ClearExpiredBroadcastLockstepSettings(state);
    if (state.net_session.role != NetRole::Host ||
        !state.net_session.lockstep_broadcast_settings.has_value()) {
        return;
    }

    const LockstepSettingsPacket packet =
        MakeLockstepSettingsPacket(state, *state.net_session.lockstep_broadcast_settings);
    const EncodedNetPacket encoded = EncodeLockstepSettings(packet);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

bool ScheduleLockstepSettingsChangeImpl(
    State& state,
    std::uint32_t input_delay_frames,
    std::uint32_t max_rollback_frames,
    bool manual,
    std::string* status_out
) {
    if (!IsInputLockstepSession(state) || state.net_session.role != NetRole::Host) {
        if (status_out != nullptr) {
            *status_out = "Only the lockstep host can schedule live settings changes.";
        }
        return false;
    }

    input_delay_frames = ClampLockstepInputDelayFrames(input_delay_frames);
    max_rollback_frames = ClampLockstepMaxRollbackFrames(max_rollback_frames);
    if (state.net_session.lockstep_pending_settings.has_value()) {
        const PendingLockstepSettings& pending = *state.net_session.lockstep_pending_settings;
        if (pending.input_delay_frames == input_delay_frames &&
            pending.max_rollback_frames == max_rollback_frames) {
            if (status_out != nullptr) {
                *status_out = "Matching lockstep settings change is already pending.";
            }
            return true;
        }
    }
    if (!state.net_session.lockstep_pending_settings.has_value() &&
        state.net_session.lockstep_input_delay_frames == input_delay_frames &&
        state.net_session.lockstep_max_rollback_frames == max_rollback_frames) {
        if (manual) {
            state.net_session.lockstep_auto_delay_enabled = false;
        }
        if (status_out != nullptr) {
            *status_out = "Lockstep settings are already active.";
        }
        return true;
    }

    const std::uint32_t frame_margin =
        std::max(state.net_session.lockstep_input_delay_frames, input_delay_frames) +
        kLockstepSettingsApplySafetyFrames;
    PendingLockstepSettings pending;
    pending.sequence = state.net_session.lockstep_next_settings_sequence++;
    pending.apply_frame = state.net_session.lockstep_next_frame_to_step + frame_margin;
    pending.input_delay_frames = input_delay_frames;
    pending.max_rollback_frames = max_rollback_frames;
    state.net_session.lockstep_pending_settings = pending;
    state.net_session.lockstep_broadcast_settings = pending;
    state.net_session.lockstep_broadcast_settings_until_frame =
        pending.apply_frame + frame_margin;
    if (manual) {
        state.net_session.lockstep_auto_delay_enabled = false;
    }
    if (status_out != nullptr) {
        *status_out = "Scheduled lockstep settings seq=" + std::to_string(pending.sequence) +
            " apply_frame=" + std::to_string(pending.apply_frame) +
            " delay=" + std::to_string(pending.input_delay_frames) +
            " rollback=" + std::to_string(pending.max_rollback_frames) + ".";
    }
    return true;
}

void ApplyPendingLockstepSettings(State& state) {
    ClearExpiredBroadcastLockstepSettings(state);
    if (!state.net_session.lockstep_pending_settings.has_value()) {
        return;
    }
    const PendingLockstepSettings pending = *state.net_session.lockstep_pending_settings;
    if (state.net_session.lockstep_next_frame_to_step < pending.apply_frame) {
        return;
    }

    state.net_session.lockstep_input_delay_frames =
        ClampLockstepInputDelayFrames(pending.input_delay_frames);
    state.net_session.lockstep_max_rollback_frames =
        ClampLockstepMaxRollbackFrames(pending.max_rollback_frames);
    state.net_session.lockstep_last_applied_settings_sequence =
        std::max(state.net_session.lockstep_last_applied_settings_sequence, pending.sequence);
    state.net_session.lockstep_pending_settings = std::nullopt;
    PruneRollbackSnapshots(state);
}

void MaybeScheduleAutoDelayChange(State& state) {
    if (state.net_session.role != NetRole::Host ||
        !state.net_session.lockstep_auto_delay_enabled ||
        state.net_session.lockstep_pending_settings.has_value() ||
        state.net_session.peers.empty()) {
        state.net_session.lockstep_auto_delay_candidate_age_frames = 0;
        return;
    }

    std::uint32_t suggested = kMinLockstepInputDelayFrames;
    for (const NetPeerState& peer : state.net_session.peers) {
        if (!peer.connected) {
            continue;
        }
        suggested = std::max(
            suggested,
            SuggestedLockstepDelayFrames(peer.estimated_ping_ms, peer.jitter_ms)
        );
    }
    suggested = ClampLockstepInputDelayFrames(suggested);
    if (suggested == state.net_session.lockstep_input_delay_frames) {
        state.net_session.lockstep_auto_delay_candidate_frames = suggested;
        state.net_session.lockstep_auto_delay_candidate_age_frames = 0;
        return;
    }
    if (state.net_session.lockstep_auto_delay_candidate_frames != suggested) {
        state.net_session.lockstep_auto_delay_candidate_frames = suggested;
        state.net_session.lockstep_auto_delay_candidate_age_frames = 1;
        return;
    }
    state.net_session.lockstep_auto_delay_candidate_age_frames += 1;
    if (state.net_session.lockstep_auto_delay_candidate_age_frames <
        kLockstepAutoDelayStableFrames) {
        return;
    }

    (void)ScheduleLockstepSettingsChangeImpl(
        state,
        suggested,
        state.net_session.lockstep_max_rollback_frames,
        false,
        nullptr
    );
    state.net_session.lockstep_auto_delay_candidate_age_frames = 0;
}

void ApplyLockstepInputsToState(
    State& state,
    const std::vector<PlayerId>& player_ids,
    const std::vector<InputFrame>& input_frames
) {
    const PlayerSlot* const primary_slot = state.players.FindPrimaryLocal();
    const PlayerId primary_player_id =
        primary_slot != nullptr ? primary_slot->player_id : kInvalidPlayerId;

    for (std::size_t i = 0; i < player_ids.size(); ++i) {
        if (player_ids[i] == primary_player_id) {
            state.playing_input_snapshot = ToPlayingInputSnapshot(input_frames[i]);
        } else {
            state.players.SetInputFrameForPlayer(player_ids[i], input_frames[i]);
        }
    }
}

void RequestRollbackFromFrame(State& state, LockstepFrame frame) {
    if (!state.net_session.lockstep_rollback_enabled ||
        frame >= state.net_session.lockstep_next_frame_to_step) {
        return;
    }
    if (!state.net_session.lockstep_rollback_requested_frame.has_value() ||
        frame < *state.net_session.lockstep_rollback_requested_frame) {
        state.net_session.lockstep_rollback_requested_frame = frame;
    }
}

void DiscardLockstepHashesFromFrame(State& state, LockstepFrame frame) {
    auto& history = state.net_session.lockstep_hash_history;
    history.erase(
        std::remove_if(
            history.begin(),
            history.end(),
            [frame](const LockstepHashRecord& record) {
                return record.frame >= frame;
            }
        ),
        history.end()
    );
    if (state.net_session.lockstep_has_recorded_hash &&
        state.net_session.lockstep_last_recorded_hash_frame >= frame) {
        state.net_session.lockstep_has_recorded_hash = !history.empty();
        if (state.net_session.lockstep_has_recorded_hash) {
            state.net_session.lockstep_last_recorded_hash_frame = history.back().frame;
        }
    }
    if (state.net_session.lockstep_has_sent_hash &&
        state.net_session.lockstep_last_sent_hash_frame >= frame) {
        state.net_session.lockstep_has_sent_hash = false;
    }
    auto& pending = state.net_session.lockstep_pending_remote_hashes;
    pending.erase(
        std::remove_if(
            pending.begin(),
            pending.end(),
            [frame](const LockstepRemoteHashRecord& record) {
                return record.frame >= frame;
            }
        ),
        pending.end()
    );
}

void SaveRollbackSnapshot(State& state, const Graphics& graphics, LockstepFrame frame) {
    if (!state.net_session.lockstep_rollback_enabled) {
        return;
    }
    const auto save_start = std::chrono::steady_clock::now();
    for (LockstepRollbackSnapshot& entry : state.net_session.lockstep_rollback_snapshots) {
        if (entry.frame == frame) {
            entry.snapshot = std::make_shared<GameplaySnapshot>(
                MakeGameplaySnapshot(state, graphics)
            );
            state.performance_stats.rollback_snapshot_save_ms += ElapsedMs(save_start);
            return;
        }
    }
    state.net_session.lockstep_rollback_snapshots.push_back(LockstepRollbackSnapshot{
        .frame = frame,
        .snapshot = std::make_shared<GameplaySnapshot>(MakeGameplaySnapshot(state, graphics)),
    });
    state.performance_stats.rollback_snapshot_save_ms += ElapsedMs(save_start);
}

void PruneRollbackSnapshots(State& state) {
    if (!state.net_session.lockstep_rollback_enabled) {
        state.net_session.lockstep_rollback_snapshots.clear();
        return;
    }
    const LockstepFrame next_frame = state.net_session.lockstep_next_frame_to_step;
    const LockstepFrame keep_frames =
        static_cast<LockstepFrame>(state.net_session.lockstep_max_rollback_frames) + 4;
    const LockstepFrame first_kept = next_frame > keep_frames ? next_frame - keep_frames : 0;
    auto& snapshots = state.net_session.lockstep_rollback_snapshots;
    snapshots.erase(
        std::remove_if(
            snapshots.begin(),
            snapshots.end(),
            [first_kept](const LockstepRollbackSnapshot& entry) {
                return entry.frame != 0 && entry.frame < first_kept;
            }
        ),
        snapshots.end()
    );
}

const GameplaySnapshot* FindRollbackSnapshot(const State& state, LockstepFrame frame) {
    for (const LockstepRollbackSnapshot& entry : state.net_session.lockstep_rollback_snapshots) {
        if (entry.frame == frame && entry.snapshot) {
            return entry.snapshot.get();
        }
    }
    return nullptr;
}

InputFrame PredictRemoteInputForFrame(
    const State& state,
    PlayerId player_id,
    LockstepFrame frame
) {
    const LockstepInputRecord* const previous =
        state.net_session.lockstep_input_buffer.FindLatestRecordBefore(player_id, frame);
    if (previous != nullptr) {
        return previous->input;
    }
    return InputFrame::New();
}

bool BuildOrPredictFrameInputs(
    State& state,
    const std::vector<PlayerId>& required_players,
    LockstepFrame frame,
    bool allow_prediction,
    std::vector<InputFrame>& out_inputs
) {
    out_inputs.clear();
    out_inputs.reserve(required_players.size());

    const std::vector<PlayerId> local_player_ids = GetLocalPlayerIds(state);
    for (PlayerId player_id : required_players) {
        const InputFrame* input = state.net_session.lockstep_input_buffer.Find(player_id, frame);
        if (input == nullptr && allow_prediction &&
            std::find(local_player_ids.begin(), local_player_ids.end(), player_id) ==
                local_player_ids.end()) {
            LockstepInputRecord prediction;
            prediction.player_id = player_id;
            prediction.frame = frame;
            prediction.input = PredictRemoteInputForFrame(state, player_id, frame);
            prediction.predicted = true;
            (void)state.net_session.lockstep_input_buffer.Store(prediction);
            input = state.net_session.lockstep_input_buffer.Find(player_id, frame);
        }
        if (input == nullptr) {
            out_inputs.clear();
            return false;
        }
        out_inputs.push_back(*input);
    }
    return true;
}

bool ReplayRollbackWindow(
    State& state,
    Graphics& graphics,
    const std::vector<PlayerId>& required_players
) {
    if (!state.net_session.lockstep_rollback_requested_frame.has_value()) {
        return true;
    }

    const LockstepFrame rollback_frame = *state.net_session.lockstep_rollback_requested_frame;
    state.net_session.lockstep_rollback_requested_frame = std::nullopt;
    const LockstepFrame target_frame = state.net_session.lockstep_next_frame_to_step;
    if (rollback_frame >= target_frame) {
        return true;
    }

    const GameplaySnapshot* const snapshot = FindRollbackSnapshot(state, rollback_frame);
    if (snapshot == nullptr) {
        (void)RequestSnapshotCatchupFromStageStart(state);
        return false;
    }

    const auto replay_start = std::chrono::steady_clock::now();
    const RollbackPresentationSnapshot presentation_snapshot =
        CaptureRollbackPresentationState(state, graphics);
    Audio replay_audio;
    const auto restore_start = std::chrono::steady_clock::now();
    RestoreGameplaySnapshot(*snapshot, state, graphics);
    state.performance_stats.rollback_snapshot_restore_ms += ElapsedMs(restore_start);
    state.net_session.lockstep_next_frame_to_step = rollback_frame;
    DiscardLockstepHashesFromFrame(state, rollback_frame);

    std::vector<InputFrame> frame_inputs;
    while (state.net_session.lockstep_next_frame_to_step < target_frame) {
        const LockstepFrame frame = state.net_session.lockstep_next_frame_to_step;
        if (!BuildOrPredictFrameInputs(
                state,
                required_players,
                frame,
                true,
                frame_inputs
            )) {
            return false;
        }
        SaveRollbackSnapshot(state, graphics, frame);
        ApplyLockstepInputsToState(state, required_players, frame_inputs);
        state.net_session.lockstep_next_frame_to_step += 1;
        StepSingleTickWithMode(state, replay_audio, graphics, SimulationTickMode::ReplayNoNetwork);
        RecordCompletedLockstepHash(state, frame);
    }
    ValidateRemoteHashesAfterReplay(state, rollback_frame, target_frame);
    RestoreRollbackPresentationState(presentation_snapshot, state, graphics);
    if (state.net_session.lockstep_last_desync_recovery_mode ==
        LockstepDesyncRecoveryMode::FatalDesync) {
        return false;
    }

    const auto replay_end = std::chrono::steady_clock::now();
    const std::chrono::duration<float, std::milli> replay_ms = replay_end - replay_start;
    const std::uint32_t span = static_cast<std::uint32_t>(target_frame - rollback_frame);
    state.net_session.lockstep_rollback_count += 1;
    state.net_session.lockstep_last_rollback_span = span;
    state.net_session.lockstep_max_rollback_span =
        std::max(state.net_session.lockstep_max_rollback_span, span);
    state.net_session.lockstep_last_rollback_replay_ms = replay_ms.count();
    state.net_session.lockstep_total_rollback_replay_ms += replay_ms.count();
    state.performance_stats.rollback_replay_ms_this_frame += replay_ms.count();
    state.performance_stats.rollback_replay_frames_this_frame += span;
    state.performance_stats.rollback_replay_ms_per_frame =
        state.performance_stats.rollback_replay_frames_this_frame == 0
            ? 0.0
            : state.performance_stats.rollback_replay_ms_this_frame /
                static_cast<double>(state.performance_stats.rollback_replay_frames_this_frame);
    PruneRollbackSnapshots(state);
    state.performance_stats.rollback_buffer_bytes = ApproxRollbackBufferBytes(state);
    return true;
}

void PumpInputLockstepPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    const auto pump_start = std::chrono::steady_clock::now();
    FlushFuzzedOutgoingPackets(transport);
    if (state.net_session.role == NetRole::Host) {
        CleanupExpiredRetainedPlayerStates(state);
        StepHostPackets(state, graphics, transport);
    } else if (state.net_session.role == NetRole::Peer) {
        StepPeerPackets(state, graphics, transport);
    }
    FlushFuzzedOutgoingPackets(transport);
    state.net_session.fuzzer_stats = transport.fuzzer_stats;
    state.performance_stats.network_pump_ms += ElapsedMs(pump_start);
}

} // namespace

bool IsInputLockstepActive(const State& state) {
    return IsInputLockstepSession(state) &&
           state.net_transport != nullptr &&
           state.net_transport->socket.IsOpen();
}

bool IsInputLockstepSession(const State& state) {
    return state.net_session.role != NetRole::Offline &&
           state.net_session.input_lockstep_enabled;
}

bool HasFatalLockstepDesync(const State& state) {
    return state.net_session.lockstep_last_desync_recovery_mode ==
        LockstepDesyncRecoveryMode::FatalDesync;
}

void ResetInputLockstepState(State& state) {
    state.net_session.lockstep_input_buffer = LockstepInputBuffer{};
    state.net_session.lockstep_next_frame_to_step = 0;
    state.net_session.lockstep_next_local_input_frame = 0;
    state.net_session.lockstep_pending_settings = std::nullopt;
    state.net_session.lockstep_broadcast_settings = std::nullopt;
    state.net_session.lockstep_broadcast_settings_until_frame = 0;
    state.net_session.lockstep_last_applied_settings_sequence = 0;
    state.net_session.lockstep_auto_delay_candidate_frames =
        state.net_session.lockstep_input_delay_frames;
    state.net_session.lockstep_auto_delay_candidate_age_frames = 0;
    state.net_session.lockstep_next_input_sequence = 1;
    state.net_session.lockstep_last_confirmed_hash_frame = 0;
    state.net_session.lockstep_last_confirmed_hash = 0;
    state.net_session.lockstep_has_confirmed_hash = false;
    state.net_session.lockstep_hash_history.clear();
    state.net_session.lockstep_remote_hash_history.clear();
    state.net_session.lockstep_pending_remote_hashes.clear();
    state.net_session.lockstep_last_recorded_hash_frame = 0;
    state.net_session.lockstep_has_recorded_hash = false;
    state.net_session.lockstep_last_sent_hash_frame = 0;
    state.net_session.lockstep_has_sent_hash = false;
    state.net_session.lockstep_hash_mismatch_count = 0;
    state.net_session.lockstep_last_mismatch_peer_id = kInvalidPlayerId;
    state.net_session.lockstep_last_mismatch_frame = 0;
    state.net_session.lockstep_last_mismatch_local_hash = 0;
    state.net_session.lockstep_last_mismatch_remote_hash = 0;
    state.net_session.lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::None;
    state.net_session.lockstep_rollback_requested_frame = std::nullopt;
    state.net_session.lockstep_rollback_snapshots.clear();
    state.net_session.lockstep_rollback_count = 0;
    state.net_session.lockstep_last_rollback_span = 0;
    state.net_session.lockstep_max_rollback_span = 0;
    state.net_session.lockstep_last_rollback_replay_ms = 0.0F;
    state.net_session.lockstep_total_rollback_replay_ms = 0.0F;
    state.net_session.lockstep_prediction_miss_count = 0;
    state.net_session.lockstep_prediction_late_match_count = 0;
    state.net_session.lockstep_last_prediction_miss_span = 0;
    state.net_session.lockstep_input_wait_block_count = 0;
}

bool ReplayPendingInputLockstepRollback(State& state, Graphics& graphics) {
    const std::vector<PlayerId> required_players = GetConnectedPlayerIds(state);
    if (required_players.empty()) {
        return false;
    }
    return ReplayRollbackWindow(state, graphics, required_players);
}

bool PrepareInputLockstepFrame(State& state, Graphics& graphics) {
    if (!IsInputLockstepActive(state)) {
        return true;
    }

    NetTransportRuntime& transport = *state.net_transport;
    transport.fuzzer_config = state.net_session.fuzzer_config;
    RecordPreviousCompletedLockstepHash(state);
    PumpInputLockstepPackets(state, graphics, transport);
    if (HasFatalLockstepDesync(state)) {
        return false;
    }
    if (state.net_session.role == NetRole::Host && transport.remotes.empty()) {
        return false;
    }
    if (state.net_session.role == NetRole::Peer && transport.join_request_pending) {
        return false;
    }

    ApplyDueLockstepSettings(state);
    UpdateLockstepAutoDelay(state);
    SendBroadcastLockstepSettings(state, transport);
    SendDueLockstepHash(state, transport);
    QueueLocalInputsThroughTargetFrame(state);
    SendLocalInputFramePacket(state, transport);
    FlushFuzzedOutgoingPackets(transport);
    state.net_session.fuzzer_stats = transport.fuzzer_stats;

    const std::vector<PlayerId> required_players = GetConnectedPlayerIds(state);
    if (required_players.empty()) {
        return false;
    }
    if (!CanStepWithoutRunningAheadOfRemoteInputs(state, required_players)) {
        state.net_session.lockstep_input_wait_block_count += 1;
        return false;
    }
    if (!ReplayPendingInputLockstepRollback(state, graphics)) {
        return false;
    }

    std::vector<InputFrame> frame_inputs;
    if (!BuildOrPredictFrameInputs(
            state,
            required_players,
            state.net_session.lockstep_next_frame_to_step,
            state.net_session.lockstep_rollback_enabled,
            frame_inputs
        )) {
        return false;
    }

    SaveRollbackSnapshot(state, graphics, state.net_session.lockstep_next_frame_to_step);
    state.performance_stats.rollback_buffer_bytes = ApproxRollbackBufferBytes(state);
    ApplyLockstepInputsToState(state, required_players, frame_inputs);
    state.net_session.lockstep_next_frame_to_step += 1;
    // Keep the full stage input log so the hard recovery path can restore the
    // frame-0 snapshot and catch up instead of ending the session.
    PruneRollbackSnapshots(state);
    return true;
}

void HandleInputFrameRecords(State& state, const InputFrameRecordsPacket& packet) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    for (std::uint32_t i = 0; i < packet.record_count; ++i) {
        const LockstepInputStoreResult result =
            state.net_session.lockstep_input_buffer.Store(MakeLockstepInputRecord(packet.records[i]));
        if (result.replaced_prediction) {
            if (result.mismatch_frame.has_value()) {
                state.net_session.lockstep_prediction_miss_count += 1;
                if (state.net_session.lockstep_next_frame_to_step > *result.mismatch_frame) {
                    state.net_session.lockstep_last_prediction_miss_span =
                        static_cast<std::uint32_t>(
                            state.net_session.lockstep_next_frame_to_step - *result.mismatch_frame
                        );
                } else {
                    state.net_session.lockstep_last_prediction_miss_span = 0;
                }
            } else {
                state.net_session.lockstep_prediction_late_match_count += 1;
            }
        }
        if (result.mismatch_frame.has_value()) {
            RequestRollbackFromFrame(state, *result.mismatch_frame);
        }
    }
}

void HandleLockstepSettingsPacket(State& state, const LockstepSettingsPacket& packet) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    if (packet.sequence <= state.net_session.lockstep_last_applied_settings_sequence) {
        return;
    }
    if (state.net_session.lockstep_pending_settings.has_value() &&
        packet.sequence < state.net_session.lockstep_pending_settings->sequence) {
        return;
    }

    PendingLockstepSettings pending;
    pending.sequence = packet.sequence;
    pending.apply_frame = packet.apply_frame;
    pending.input_delay_frames = ClampLockstepInputDelayFrames(packet.input_delay_frames);
    pending.max_rollback_frames = ClampLockstepMaxRollbackFrames(packet.max_rollback_frames);
    state.net_session.lockstep_pending_settings = pending;
    state.net_session.lockstep_next_settings_sequence =
        std::max(state.net_session.lockstep_next_settings_sequence, packet.sequence + 1);
}

void HandleLockstepHashPacket(State& state, const LockstepHashNetPacket& packet) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    if (packet.sender_peer_id == state.net_session.local_player_id) {
        return;
    }
    StoreOrCompareRemoteHash(state, LockstepRemoteHashRecord{
        .peer_id = static_cast<PlayerId>(packet.sender_peer_id),
        .frame = packet.frame,
        .hash = packet.hash,
    });
}

bool ScheduleLockstepSettingsChange(
    State& state,
    std::uint32_t input_delay_frames,
    std::uint32_t max_rollback_frames,
    std::string* status_out
) {
    return ScheduleLockstepSettingsChangeImpl(
        state,
        input_delay_frames,
        max_rollback_frames,
        true,
        status_out
    );
}

void ApplyDueLockstepSettings(State& state) {
    ApplyPendingLockstepSettings(state);
}

void UpdateLockstepAutoDelay(State& state) {
    MaybeScheduleAutoDelayChange(state);
}

void RelayInputFrameRecordsToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const InputFrameRecordsPacket& packet
) {
    const EncodedNetPacket encoded = EncodeInputFrameRecords(packet);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, source_endpoint)) {
            continue;
        }
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

void RelayLockstepHashToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const LockstepHashNetPacket& packet
) {
    const EncodedNetPacket encoded = EncodeLockstepHash(packet);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, source_endpoint)) {
            continue;
        }
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

} // namespace splonks::network

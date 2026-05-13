#include "network/net_lobby_internal.hpp"

#include "inputs.hpp"
#include "simulation_snapshot.hpp"
#include "state.hpp"
#include "step.hpp"

#include <algorithm>
#include <chrono>
#include <memory>

namespace splonks::network {

namespace {

constexpr LockstepFrame kInputHistoryFrames = 12;

struct RollbackPresentationSnapshot {
    ParticleSystem particles;
    AudioEmitterManager audio_emitters;
    StageLighting stage_lighting;
    Vec2 audio_listener_world_pos = Vec2::New(0.0F, 0.0F);
    std::optional<Vec2> gameplay_camera_anchor_world_pos;
    Vec2 play_cam_pos = Vec2::New(0.0F, 0.0F);
};

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

void SaveRollbackSnapshot(State& state, const Graphics& graphics, LockstepFrame frame) {
    if (!state.net_session.lockstep_rollback_enabled) {
        return;
    }
    for (LockstepRollbackSnapshot& entry : state.net_session.lockstep_rollback_snapshots) {
        if (entry.frame == frame) {
            entry.snapshot = std::make_shared<GameplaySnapshot>(
                MakeGameplaySnapshot(state, graphics)
            );
            return;
        }
    }
    state.net_session.lockstep_rollback_snapshots.push_back(LockstepRollbackSnapshot{
        .frame = frame,
        .snapshot = std::make_shared<GameplaySnapshot>(MakeGameplaySnapshot(state, graphics)),
    });
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
                return entry.frame < first_kept;
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
        return false;
    }

    const auto replay_start = std::chrono::steady_clock::now();
    const RollbackPresentationSnapshot presentation_snapshot =
        CaptureRollbackPresentationState(state, graphics);
    Audio replay_audio;
    RestoreGameplaySnapshot(*snapshot, state, graphics);
    state.net_session.lockstep_next_frame_to_step = rollback_frame;

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
    }
    RestoreRollbackPresentationState(presentation_snapshot, state, graphics);

    const auto replay_end = std::chrono::steady_clock::now();
    const std::chrono::duration<float, std::milli> replay_ms = replay_end - replay_start;
    const std::uint32_t span = static_cast<std::uint32_t>(target_frame - rollback_frame);
    state.net_session.lockstep_rollback_count += 1;
    state.net_session.lockstep_last_rollback_span = span;
    state.net_session.lockstep_max_rollback_span =
        std::max(state.net_session.lockstep_max_rollback_span, span);
    state.net_session.lockstep_last_rollback_replay_ms = replay_ms.count();
    state.net_session.lockstep_total_rollback_replay_ms += replay_ms.count();
    PruneRollbackSnapshots(state);
    return true;
}

void PumpInputLockstepPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    FlushFuzzedOutgoingPackets(transport);
    if (state.net_session.role == NetRole::Host) {
        CleanupExpiredRetainedPlayerStates(state);
        StepHostPackets(state, graphics, transport);
    } else if (state.net_session.role == NetRole::Peer) {
        StepPeerPackets(state, graphics, transport);
    }
    FlushFuzzedOutgoingPackets(transport);
    state.net_session.fuzzer_stats = transport.fuzzer_stats;
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

void ResetInputLockstepState(State& state) {
    state.net_session.lockstep_input_buffer = LockstepInputBuffer{};
    state.net_session.lockstep_next_frame_to_step = 0;
    state.net_session.lockstep_next_local_input_frame = 0;
    state.net_session.lockstep_next_input_sequence = 1;
    state.net_session.lockstep_last_confirmed_hash_frame = 0;
    state.net_session.lockstep_last_confirmed_hash = 0;
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

bool PrepareInputLockstepFrame(State& state, Graphics& graphics) {
    if (!IsInputLockstepActive(state)) {
        return true;
    }

    NetTransportRuntime& transport = *state.net_transport;
    transport.fuzzer_config = state.net_session.fuzzer_config;
    PumpInputLockstepPackets(state, graphics, transport);
    if (state.net_session.role == NetRole::Host && transport.remotes.empty()) {
        return false;
    }
    if (state.net_session.role == NetRole::Peer && transport.join_request_pending) {
        return false;
    }

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
    if (!ReplayRollbackWindow(state, graphics, required_players)) {
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
    ApplyLockstepInputsToState(state, required_players, frame_inputs);
    state.net_session.lockstep_next_frame_to_step += 1;
    const LockstepFrame input_history_frames = std::max<LockstepFrame>(
        kInputHistoryFrames,
        static_cast<LockstepFrame>(state.net_session.lockstep_max_rollback_frames) + 4
    );
    if (state.net_session.lockstep_next_frame_to_step > input_history_frames) {
        state.net_session.lockstep_input_buffer.ClearBefore(
            state.net_session.lockstep_next_frame_to_step - input_history_frames
        );
    }
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

} // namespace splonks::network

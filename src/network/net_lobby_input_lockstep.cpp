#include "network/net_lobby_internal.hpp"

#include "ent/spec_restore.hpp"
#include "inputs.hpp"
#include "math_types.hpp"
#include "sim/fxp.hpp"
#include "simulation_snapshot.hpp"
#include "state_fingerprint.hpp"
#include "state.hpp"
#include "step.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

namespace splonks::network {

void RelinkPlayerNetEnts(State& state);

namespace {

constexpr LockstepFrame kInputHistoryFrames = 12;
constexpr std::size_t kMaxPendingRemoteHashes = 128;
constexpr std::uint32_t kInputRecordFlagCanonical = 1U << 31U;
constexpr std::uint32_t kInputRecordFlagArbitratedMissing = 1U << 30U;
constexpr std::uint32_t kDesyncReplayMagic = 0x53445250U; // SDRP
constexpr std::uint32_t kDesyncReplayVersion = 3;
// Snapshot chunks are UDP packets. Keep this below the packet pump receive budget
// so a catchup burst does not overrun peer socket buffers and permanently miss chunks.
constexpr std::uint32_t kSnapshotResyncChunksPerPump = 4;
constexpr std::uint32_t kJoinBarrierChunksPerPump = 8;
constexpr std::size_t kReplayInputReserveChunk = 4096;
constexpr std::uint32_t kMaxReplayDumpsPerRun = 32;
constexpr LockstepFrame kReplayDumpMinFrameGap = 60;

void SortUniqueValidPlayerIds(std::vector<PlayerId>& player_ids) {
    player_ids.erase(
        std::remove(player_ids.begin(), player_ids.end(), kInvalidPlayerId),
        player_ids.end()
    );
    std::sort(player_ids.begin(), player_ids.end());
    player_ids.erase(
        std::unique(player_ids.begin(), player_ids.end()),
        player_ids.end()
    );
}

enum class LockstepHashContext : std::uint8_t {
    Normal,
    Rollback,
};

void WriteReplayBytes(std::ostream& out, const void* data, std::size_t size) {
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void WriteReplayByte(std::ostream& out, std::uint8_t value) {
    const char byte = static_cast<char>(value);
    out.write(&byte, 1);
}

void WriteReplayUint16(std::ostream& out, std::uint16_t value) {
    for (unsigned shift = 0; shift < 16; shift += 8) {
        WriteReplayByte(out, static_cast<std::uint8_t>(
            (value >> shift) & static_cast<std::uint16_t>(0xFFU)
        ));
    }
}

void WriteReplayUint32(std::ostream& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        WriteReplayByte(out, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void WriteReplayUint64(std::ostream& out, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        WriteReplayByte(out, static_cast<std::uint8_t>((value >> shift) & 0xFFULL));
    }
}

void WriteReplayString(std::ostream& out, const std::string& value) {
    const std::uint32_t size = static_cast<std::uint32_t>(value.size());
    WriteReplayUint32(out, size);
    if (size > 0) {
        out.write(value.data(), static_cast<std::streamsize>(size));
    }
}

void ResetLockstepReplayCapture(State& state) {
    state.net_session.lockstep_replay_capture = LockstepReplayCapture{};
}

void EnsureLockstepReplayCaptureStarted(State& state) {
    if (!state.net_session.lockstep_replay_capture_enabled ||
        !state.net_session.lockstep_replay_capture.initial_snapshot.empty()) {
        return;
    }
    LockstepReplayCapture& capture = state.net_session.lockstep_replay_capture;
    capture.stage_instance_id = state.net_session.stage_instance_id;
    capture.quest_id = state.net_session.quest_id;
    capture.quest_stage_id = state.net_session.quest_stage_id;
    capture.stage_seed = state.net_session.stage_seed;
    capture.start_frame = state.net_session.lockstep_next_frame_to_step;
    capture.initial_snapshot = SerializeSimSnapshotToBytes(MakeSimSnapshot(state));
    capture.inputs.clear();
    capture.last_dump_frame = 0;
    capture.dump_count = 0;
}

void RecordLockstepReplayInputs(
    State& state,
    LockstepFrame frame,
    const std::vector<PlayerId>& player_ids,
    const std::vector<InputFrame>& input_frames
) {
    EnsureLockstepReplayCaptureStarted(state);
    if (state.net_session.lockstep_replay_capture.initial_snapshot.empty()) {
        return;
    }
    LockstepReplayCapture& capture = state.net_session.lockstep_replay_capture;
    const std::size_t required_size = capture.inputs.size() + input_frames.size();
    if (required_size > capture.inputs.capacity()) {
        const std::size_t chunked_capacity =
            ((required_size + kReplayInputReserveChunk - 1) / kReplayInputReserveChunk) *
            kReplayInputReserveChunk;
        capture.inputs.reserve(chunked_capacity);
    }
    for (std::size_t i = 0; i < player_ids.size() && i < input_frames.size(); ++i) {
        capture.inputs.push_back(LockstepReplayInputRecord{
            .player_id = player_ids[i],
            .frame = frame,
            .input_flags = PackInputFrame(input_frames[i]),
            .mouse_x = input_frames[i].mouse_pos.x,
            .mouse_y = input_frames[i].mouse_pos.y,
        });
    }
}

std::string RoleSlug(NetRole role) {
    switch (role) {
    case NetRole::Host:
        return "host";
    case NetRole::Peer:
        return "peer";
    case NetRole::Offline:
        return "offline";
    }
    return "unknown";
}

void DumpLockstepReplayCaptureOnDesync(
    State& state,
    const LockstepHashRecord& local,
    const LockstepRemoteHashRecord& remote
) {
    LockstepReplayCapture& capture = state.net_session.lockstep_replay_capture;
    if (!state.net_session.lockstep_replay_capture_enabled ||
        capture.initial_snapshot.empty()) {
        return;
    }
    if (capture.dump_count >= kMaxReplayDumpsPerRun) {
        return;
    }
    if (capture.dump_count > 0 &&
        remote.frame < capture.last_dump_frame + kReplayDumpMinFrameGap) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::ostringstream path;
    path << "logs/desync_replay_" << RoleSlug(state.net_session.role)
         << "_stage" << state.net_session.stage_instance_id
         << "_frame" << remote.frame
         << "_seq" << capture.dump_count
         << "_" << millis << ".sdrp";

    std::ofstream out(path.str(), std::ios::binary);
    if (!out.is_open()) {
        return;
    }

    WriteReplayUint32(out, kDesyncReplayMagic);
    WriteReplayUint32(out, kDesyncReplayVersion);
    WriteReplayUint64(out, capture.stage_instance_id);
    WriteReplayUint32(out, capture.stage_seed);
    WriteReplayUint64(out, capture.start_frame);
    WriteReplayString(out, capture.quest_id);
    WriteReplayString(out, capture.quest_stage_id);
    WriteReplayUint32(out, remote.peer_id);
    WriteReplayUint64(out, remote.frame);
    WriteReplayUint64(out, local.hash);
    WriteReplayUint64(out, remote.hash);
    WriteReplayUint64(out, local.component_root);
    WriteReplayUint64(out, remote.component_root);
    WriteReplayUint64(out, local.component_stage);
    WriteReplayUint64(out, remote.component_stage);
    WriteReplayUint64(out, local.component_players);
    WriteReplayUint64(out, remote.component_players);
    WriteReplayUint64(out, local.component_tools);
    WriteReplayUint64(out, remote.component_tools);
    WriteReplayUint64(out, local.component_ents);
    WriteReplayUint64(out, remote.component_ents);

    const std::uint32_t snapshot_size =
        static_cast<std::uint32_t>(capture.initial_snapshot.size());
    WriteReplayUint32(out, snapshot_size);
    if (snapshot_size > 0) {
        out.write(
            reinterpret_cast<const char*>(capture.initial_snapshot.data()),
            static_cast<std::streamsize>(capture.initial_snapshot.size())
        );
    }

    const std::uint64_t input_count = capture.inputs.size();
    WriteReplayUint64(out, input_count);
    for (const LockstepReplayInputRecord& input : capture.inputs) {
        WriteReplayUint32(out, input.player_id);
        WriteReplayUint64(out, input.frame);
        WriteReplayUint32(out, input.input_flags);
        WriteReplayUint32(out, input.mouse_x);
        WriteReplayUint32(out, input.mouse_y);
    }

    const std::vector<NetworkEntFingerprint> local_ent_hashes =
        local.component_ents != remote.component_ents
            ? ComputeNetworkEntFingerprints(state)
            : std::vector<NetworkEntFingerprint>{};
    const std::uint64_t ent_hash_count = local_ent_hashes.size();
    WriteReplayUint64(out, ent_hash_count);
    for (const NetworkEntFingerprint& ent_hash : local_ent_hashes) {
        WriteReplayUint64(out, ent_hash.net_ent_id);
        WriteReplayUint16(out, ent_hash.type);
        WriteReplayUint64(out, ent_hash.hash);
    }

    if (!out.good()) {
        return;
    }
    capture.last_dump_frame = remote.frame;
    capture.dump_count += 1;
    state.net_session.lockstep_last_desync_replay_path = path.str();
}

struct RollbackPresentationSnapshot {
    ParticleSystem particles;
    AudioEmitterManager audio_emitters;
    StageLighting stage_lighting;
    Vec2 audio_listener_world_pos = Vec2::New(0.0F, 0.0F);
    std::optional<Vec2> gameplay_camera_anchor_world_pos;
    std::optional<VID> controlled_ent_vid;
    std::optional<PlayerId> spectator_target_player_id;
    Vec2 play_cam_pos = Vec2::New(0.0F, 0.0F);
};

void PruneRollbackSnapshots(State& state);
void RequestRollbackFromFrame(State& state, LockstepFrame frame);
const SimSnapshot* FindRollbackSnapshot(const State& state, LockstepFrame frame);
std::vector<PlayerId> GetConnectedPlayerIds(const State& state);
bool AllRequiredInputsConfirmedForFrame(const State& state, LockstepFrame frame);

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
    const int frames = CeilToInt((one_way_ms + jitter_margin_ms) / kNetworkFrameMs + kSafetyFrames);
    return ClampLockstepInputDelayFrames(static_cast<std::uint32_t>(std::max(0, frames)));
}

std::uint64_t ApproxRollbackBufferBytes(const State& state) {
    std::uint64_t bytes = 0;
    for (const LockstepRollbackSnapshot& entry : state.net_session.lockstep_rollback_snapshots) {
        if (entry.snapshot) {
            bytes += static_cast<std::uint64_t>(sizeof(SimSnapshot));
            bytes += static_cast<std::uint64_t>(entry.snapshot->ents.ents.capacity()) *
                     static_cast<std::uint64_t>(sizeof(Ent));
            bytes += static_cast<std::uint64_t>(entry.snapshot->stage.tiles.capacity()) *
                     static_cast<std::uint64_t>(sizeof(std::vector<Tile>));
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

void ClearLockstepHashState(State& state) {
    state.net_session.lockstep_hash_history.clear();
    state.net_session.lockstep_remote_hash_history.clear();
    state.net_session.lockstep_pending_remote_hashes.clear();
    state.net_session.lockstep_has_recorded_hash = false;
    state.net_session.lockstep_has_sent_hash = false;
    state.net_session.lockstep_has_confirmed_hash = false;
    state.net_session.lockstep_last_recorded_hash_frame = 0;
    state.net_session.lockstep_last_sent_hash_frame = 0;
    state.net_session.lockstep_last_confirmed_hash_frame = 0;
    state.net_session.lockstep_last_confirmed_hash = 0;
    state.net_session.lockstep_last_mismatch_local_ent_hashes.clear();
    ResetLockstepReplayCapture(state);
}

void SetPostCatchupHashQuietWindow(State& state, LockstepFrame resume_frame) {
    const LockstepFrame input_delay =
        static_cast<LockstepFrame>(state.net_session.lockstep_input_delay_frames);
    const LockstepFrame hash_settle_frames =
        static_cast<LockstepFrame>(state.net_session.lockstep_hash_send_interval_frames) * 8U;
    state.net_session.lockstep_hash_ignore_through_frame =
        resume_frame + std::max<LockstepFrame>(input_delay * 2U + 2U, hash_settle_frames);
}

void ClearSnapshotResyncState(State& state) {
    state.net_session.lockstep_snapshot_resync_pending_request = false;
    state.net_session.lockstep_snapshot_resync_target_peer_id = kInvalidPlayerId;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = 0;
    state.net_session.lockstep_snapshot_resync_frame = 0;
    state.net_session.lockstep_snapshot_resync_chunk_count = 0;
    state.net_session.lockstep_snapshot_resync_total_bytes = 0;
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = 0;
    state.net_session.lockstep_snapshot_resync_bytes.clear();
    state.net_session.lockstep_snapshot_resync_received_chunks.clear();
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = false;
    state.net_session.lockstep_snapshot_resync_queue.clear();
}

bool SnapshotResyncTransferInProgress(const State& state) {
    return state.net_session.lockstep_snapshot_resync_pending_request ||
           state.net_session.lockstep_snapshot_resync_waiting_for_ack ||
           state.net_session.lockstep_snapshot_resync_active_transfer_id != 0 ||
           !state.net_session.lockstep_snapshot_resync_bytes.empty();
}

void QueueSnapshotResyncTarget(State& state, PlayerId target_peer_id) {
    if (target_peer_id == kInvalidPlayerId ||
        target_peer_id == state.net_session.lockstep_snapshot_resync_target_peer_id) {
        return;
    }
    std::vector<PlayerId>& queue = state.net_session.lockstep_snapshot_resync_queue;
    if (std::find(queue.begin(), queue.end(), target_peer_id) == queue.end()) {
        queue.push_back(target_peer_id);
    }
}

std::optional<PlayerId> PopSnapshotResyncTarget(State& state) {
    std::vector<PlayerId>& queue = state.net_session.lockstep_snapshot_resync_queue;
    if (queue.empty()) {
        return std::nullopt;
    }
    const PlayerId target_peer_id = queue.front();
    queue.erase(queue.begin());
    return target_peer_id;
}

void StartOrQueueHostSnapshotResync(State& state, PlayerId target_peer_id) {
    if (target_peer_id == kInvalidPlayerId) {
        return;
    }
    const bool same_target =
        state.net_session.lockstep_snapshot_resync_target_peer_id == target_peer_id ||
        state.net_session.lockstep_snapshot_resync_target_peer_id == kInvalidPlayerId;
    const bool transfer_in_progress = SnapshotResyncTransferInProgress(state);
    if (state.net_session.lockstep_last_desync_recovery_mode ==
            LockstepDesyncRecoveryMode::SnapshotCatchup &&
        transfer_in_progress) {
        if (!same_target && state.net_session.role == NetRole::Host) {
            QueueSnapshotResyncTarget(state, target_peer_id);
        }
        return;
    }

    state.net_session.lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::SnapshotCatchup;
    state.net_session.lockstep_snapshot_resync_pending_request = true;
    state.net_session.lockstep_snapshot_resync_target_peer_id = target_peer_id;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = 0;
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = 0;
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = false;
    state.net_session.lockstep_snapshot_resync_bytes.clear();
    state.net_session.lockstep_snapshot_resync_received_chunks.clear();
}

const NetEndpoint* FindRemoteEndpointForPlayer(
    const NetTransportRuntime& transport,
    PlayerId player_id
) {
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (std::find(remote.player_ids.begin(), remote.player_ids.end(), player_id) !=
            remote.player_ids.end()) {
            return &remote.endpoint;
        }
    }
    return nullptr;
}

void SendSnapshotResyncStoredChunks(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    std::uint32_t chunks_per_pump,
    bool track_join_progress
) {
    if (state.net_session.lockstep_snapshot_resync_bytes.empty() ||
        state.net_session.lockstep_snapshot_resync_chunk_count == 0 ||
        state.net_session.lockstep_snapshot_resync_total_bytes == 0 ||
        state.net_session.lockstep_snapshot_resync_active_transfer_id == 0) {
        return;
    }

    const std::vector<std::uint8_t>& bytes = state.net_session.lockstep_snapshot_resync_bytes;
    const std::uint32_t chunk_count = state.net_session.lockstep_snapshot_resync_chunk_count;
    std::uint32_t chunk_index =
        state.net_session.lockstep_snapshot_resync_next_chunk_to_send % chunk_count;
    for (std::uint32_t sent = 0; sent < chunks_per_pump; ++sent) {
        const std::size_t begin =
            static_cast<std::size_t>(chunk_index) * kNetSnapshotChunkPayloadBytes;
        if (begin >= bytes.size()) {
            chunk_index = 0;
            continue;
        }
        const std::size_t remaining = bytes.size() - begin;
        const std::size_t payload_bytes = std::min<std::size_t>(
            remaining,
            kNetSnapshotChunkPayloadBytes
        );

        SnapshotResyncChunkPacket packet;
        packet.stage_instance_id = state.net_session.stage_instance_id;
        packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
        packet.transfer_id = state.net_session.lockstep_snapshot_resync_active_transfer_id;
        packet.chunk_index = chunk_index;
        packet.chunk_count = state.net_session.lockstep_snapshot_resync_chunk_count;
        packet.total_bytes = state.net_session.lockstep_snapshot_resync_total_bytes;
        packet.payload_bytes = static_cast<std::uint32_t>(payload_bytes);
        packet.snapshot_frame = state.net_session.lockstep_snapshot_resync_frame;
        std::copy_n(bytes.data() + begin, payload_bytes, packet.payload.begin());
        SendEncodedPacket(transport, endpoint, EncodeSnapshotResyncChunk(packet));
        if (track_join_progress &&
            state.net_session.join_barrier_chunks_done < chunk_count) {
            state.net_session.join_barrier_chunks_done += 1U;
            const std::size_t done_bytes = std::min<std::size_t>(
                bytes.size(),
                static_cast<std::size_t>(state.net_session.join_barrier_chunks_done) *
                    kNetSnapshotChunkPayloadBytes
            );
            state.net_session.join_barrier_bytes_done =
                static_cast<std::uint32_t>(done_bytes);
        }
        chunk_index = (chunk_index + 1U) % chunk_count;
    }
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = chunk_index;
}

void SendSnapshotResyncRequest(State& state, NetTransportRuntime& transport) {
    SnapshotResyncRequestPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    packet.mismatch_frame = state.net_session.lockstep_last_mismatch_frame;
    SendEncodedPacket(transport, transport.host_endpoint, EncodeSnapshotResyncRequest(packet));
    state.net_session.lockstep_snapshot_resync_pending_request = false;
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    state.net_session.lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::SnapshotCatchup;
}

void SendSnapshotResyncAck(
    State& state,
    NetTransportRuntime& transport,
    std::uint32_t transfer_id,
    LockstepFrame snapshot_frame,
    bool success
) {
    SnapshotResyncAckPacket ack;
    ack.stage_instance_id = state.net_session.stage_instance_id;
    ack.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    ack.transfer_id = transfer_id;
    ack.snapshot_frame = snapshot_frame;
    ack.success = success ? 1 : 0;
    SendEncodedPacket(transport, transport.host_endpoint, EncodeSnapshotResyncAck(ack));
    state.net_session.lockstep_snapshot_resync_last_acked_transfer_id = transfer_id;
    state.net_session.lockstep_snapshot_resync_last_acked_frame = snapshot_frame;
    state.net_session.lockstep_snapshot_resync_last_ack_success = ack.success;
}

void SendSnapshotResyncChunksToEndpoint(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    PlayerId target_peer_id
) {
    (void)graphics;
    const std::uint32_t transfer_id = state.net_session.lockstep_snapshot_resync_next_transfer_id++;
    const LockstepFrame snapshot_frame = state.net_session.lockstep_next_frame_to_step;
    const std::vector<std::uint8_t> bytes =
        SerializeSimSnapshotToBytes(MakeSimSnapshot(state));
    const std::uint32_t chunk_count = static_cast<std::uint32_t>(
        (bytes.size() + kNetSnapshotChunkPayloadBytes - 1) / kNetSnapshotChunkPayloadBytes
    );

    state.net_session.lockstep_snapshot_resync_pending_request = false;
    state.net_session.lockstep_snapshot_resync_target_peer_id = target_peer_id;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = transfer_id;
    state.net_session.lockstep_snapshot_resync_frame = snapshot_frame;
    state.net_session.lockstep_snapshot_resync_chunk_count = chunk_count;
    state.net_session.lockstep_snapshot_resync_total_bytes =
        static_cast<std::uint32_t>(bytes.size());
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = 0;
    state.net_session.lockstep_snapshot_resync_bytes = bytes;
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = true;
    state.net_session.lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::SnapshotCatchup;
    SendSnapshotResyncStoredChunks(
        state,
        transport,
        endpoint,
        kSnapshotResyncChunksPerPump,
        false
    );
}

void SendPendingSnapshotResync(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    constexpr std::uint32_t kSnapshotResyncRetryTicks = 30;

    if (!state.net_session.lockstep_snapshot_resync_pending_request) {
        if (state.net_session.lockstep_last_desync_recovery_mode !=
            LockstepDesyncRecoveryMode::SnapshotCatchup) {
            return;
        }
        if (state.net_session.lockstep_snapshot_resync_waiting_for_ack) {
            const PlayerId target_peer_id =
                state.net_session.lockstep_snapshot_resync_target_peer_id;
            const NetEndpoint* const endpoint =
                FindRemoteEndpointForPlayer(transport, target_peer_id);
            if (endpoint == nullptr) {
                state.net_session.lockstep_last_desync_recovery_mode =
                    LockstepDesyncRecoveryMode::FatalDesync;
                state.net_session.lockstep_snapshot_resync_waiting_for_ack = false;
                return;
            }
            SendSnapshotResyncStoredChunks(
                state,
                transport,
                *endpoint,
                kSnapshotResyncChunksPerPump,
                false
            );
            state.net_session.lockstep_snapshot_resync_retry_ticks += 1;
            return;
        }
        state.net_session.lockstep_snapshot_resync_retry_ticks += 1;
        if (state.net_session.lockstep_snapshot_resync_retry_ticks < kSnapshotResyncRetryTicks) {
            return;
        }
        state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
        if (state.net_session.role == NetRole::Peer) {
            if (state.net_session.lockstep_snapshot_resync_active_transfer_id != 0 &&
                !state.net_session.lockstep_snapshot_resync_bytes.empty()) {
                return;
            }
            SendSnapshotResyncRequest(state, transport);
            return;
        }
        return;
    }
    if (state.net_session.role == NetRole::Peer) {
        SendSnapshotResyncRequest(state, transport);
        return;
    }
    const PlayerId target_peer_id = state.net_session.lockstep_snapshot_resync_target_peer_id;
    const NetEndpoint* const endpoint = FindRemoteEndpointForPlayer(transport, target_peer_id);
    if (endpoint == nullptr) {
        state.net_session.lockstep_last_desync_recovery_mode =
            LockstepDesyncRecoveryMode::FatalDesync;
        state.net_session.lockstep_snapshot_resync_pending_request = false;
        state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
        return;
    }
    SendSnapshotResyncChunksToEndpoint(state, graphics, transport, *endpoint, target_peer_id);
}

bool IsSnapshotResyncBlocking(const State& state) {
    return state.net_session.lockstep_snapshot_resync_pending_request ||
           state.net_session.lockstep_snapshot_resync_waiting_for_ack ||
           state.net_session.lockstep_last_desync_recovery_mode ==
               LockstepDesyncRecoveryMode::SnapshotCatchup;
}

void ClearJoinBarrierTransfer(State& state) {
    state.net_session.join_barrier_active_peer_id = kInvalidPlayerId;
    state.net_session.join_barrier_transfer_id = 0;
    state.net_session.join_barrier_snapshot_frame = 0;
    state.net_session.join_barrier_chunk_count = 0;
    state.net_session.join_barrier_chunks_done = 0;
    state.net_session.join_barrier_total_bytes = 0;
    state.net_session.join_barrier_bytes_done = 0;
    state.net_session.lockstep_snapshot_resync_target_peer_id = kInvalidPlayerId;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = 0;
    state.net_session.lockstep_snapshot_resync_frame = 0;
    state.net_session.lockstep_snapshot_resync_chunk_count = 0;
    state.net_session.lockstep_snapshot_resync_total_bytes = 0;
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = 0;
    state.net_session.lockstep_snapshot_resync_bytes.clear();
    state.net_session.lockstep_snapshot_resync_received_chunks.clear();
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = false;
}

void ClearJoinBarrierFields(State& state) {
    state.net_session.join_barrier_active = false;
    state.net_session.join_barrier_phase = JoinBarrierPhase::None;
    state.net_session.join_barrier_queue.clear();
    state.net_session.join_barrier_topology_ack_peers.clear();
    state.net_session.join_barrier_joined_player_ids.clear();
    state.net_session.join_barrier_removed_player_ids.clear();
    ClearJoinBarrierTransfer(state);
}

void ClearJoinBarrier(State& state) {
    const LockstepFrame resume_frame = state.net_session.lockstep_next_frame_to_step;
    ClearJoinBarrierFields(state);
    ClearLockstepHashState(state);
    SetPostCatchupHashQuietWindow(state, resume_frame);
}

bool IsJoinBarrierBlocking(const State& state) {
    return state.net_session.join_barrier_active &&
           state.net_session.join_barrier_phase != JoinBarrierPhase::None;
}

bool PacketMatchesCurrentOrAcceptedNextStage(const State& state, StageInstanceId stage_instance_id) {
    if (stage_instance_id == state.net_session.stage_instance_id) {
        return true;
    }
    return state.net_session.role == NetRole::Peer &&
           state.net_session.run_restart_pending &&
           !state.net_session.run_restart_applied_locally &&
           stage_instance_id == state.net_session.stage_instance_id + 1 &&
           stage_instance_id == state.net_session.run_restart_last_packet_stage_instance_id;
}

void QueueJoinBarrierPeer(State& state, PlayerId target_peer_id) {
    if (target_peer_id == kInvalidPlayerId ||
        target_peer_id == state.net_session.local_player_id) {
        return;
    }
    std::vector<PlayerId>& queue = state.net_session.join_barrier_queue;
    if (std::find(queue.begin(), queue.end(), target_peer_id) == queue.end()) {
        queue.push_back(target_peer_id);
    }
}

void QueueJoinBarrierTopologyAck(State& state, PlayerId target_peer_id) {
    if (target_peer_id == kInvalidPlayerId ||
        target_peer_id == state.net_session.local_player_id) {
        return;
    }
    std::vector<PlayerId>& peers = state.net_session.join_barrier_topology_ack_peers;
    if (std::find(peers.begin(), peers.end(), target_peer_id) == peers.end()) {
        peers.push_back(target_peer_id);
        SortUniqueValidPlayerIds(peers);
    }
}

std::vector<PlayerId> JoinBarrierEndpointTargetsForPlayers(
    const NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids
) {
    std::vector<PlayerId> targets;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (remote.player_ids.empty()) {
            continue;
        }
        const bool endpoint_has_joined_player =
            std::any_of(
                remote.player_ids.begin(),
                remote.player_ids.end(),
                [&](PlayerId player_id) {
                    return std::find(player_ids.begin(), player_ids.end(), player_id) !=
                           player_ids.end();
                }
            );
        if (!endpoint_has_joined_player) {
            continue;
        }
        const PlayerId target = remote.player_ids.front();
        if (target != kInvalidPlayerId &&
            std::find(targets.begin(), targets.end(), target) == targets.end()) {
            targets.push_back(target);
        }
    }

    if (targets.empty()) {
        for (const PlayerId player_id : player_ids) {
            if (player_id != kInvalidPlayerId) {
                targets.push_back(player_id);
                break;
            }
        }
    }
    return targets;
}

std::optional<PlayerId> PopJoinBarrierPeer(State& state) {
    std::vector<PlayerId>& queue = state.net_session.join_barrier_queue;
    if (queue.empty()) {
        return std::nullopt;
    }
    const PlayerId target_peer_id = queue.front();
    queue.erase(queue.begin());
    return target_peer_id;
}

JoinBarrierStatusPacket BuildJoinBarrierStatusPacket(const State& state) {
    JoinBarrierStatusPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    packet.barrier_id = state.net_session.join_barrier_id;
    packet.active = state.net_session.join_barrier_active ? 1 : 0;
    packet.phase = static_cast<std::uint8_t>(state.net_session.join_barrier_phase);
    packet.active_player_id = state.net_session.join_barrier_active_peer_id;
    packet.queued_peer_count = static_cast<std::uint32_t>(std::min<std::size_t>(
        state.net_session.join_barrier_queue.size(),
        packet.queued_peer_ids.size()
    ));
    for (std::uint32_t i = 0; i < packet.queued_peer_count; ++i) {
        packet.queued_peer_ids[i] = state.net_session.join_barrier_queue[i];
    }
    packet.transfer_id = state.net_session.join_barrier_transfer_id;
    packet.snapshot_frame = state.net_session.join_barrier_snapshot_frame;
    packet.chunk_count = state.net_session.join_barrier_chunk_count;
    packet.chunks_done = state.net_session.join_barrier_chunks_done;
    packet.total_bytes = state.net_session.join_barrier_total_bytes;
    packet.bytes_done = state.net_session.join_barrier_bytes_done;
    return packet;
}

void BroadcastJoinBarrierStatus(State& state, NetTransportRuntime& transport) {
    if (state.net_session.role != NetRole::Host || !state.net_session.join_barrier_active) {
        return;
    }
    const EncodedNetPacket encoded = EncodeJoinBarrierStatus(BuildJoinBarrierStatusPacket(state));
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

void BroadcastJoinBarrierResume(State& state, NetTransportRuntime& transport) {
    JoinBarrierResumePacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    packet.barrier_id = state.net_session.join_barrier_id;
    packet.resume_frame = state.net_session.lockstep_next_frame_to_step;
    const EncodedNetPacket encoded = EncodeJoinBarrierResume(packet);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

JoinBarrierTopologyPacket BuildJoinBarrierTopologyPacket(const State& state) {
    JoinBarrierTopologyPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    packet.barrier_id = state.net_session.join_barrier_id;
    packet.barrier_frame = state.net_session.lockstep_next_frame_to_step;
    packet.player_count = static_cast<std::uint32_t>(std::min<std::size_t>(
        state.net_session.join_barrier_joined_player_ids.size(),
        packet.player_ids.size()
    ));
    for (std::uint32_t i = 0; i < packet.player_count; ++i) {
        const PlayerId player_id = state.net_session.join_barrier_joined_player_ids[i];
        packet.player_ids[i] = player_id;
        Vec2 pos = GetRemoteSpawnPos(state);
        if (const PlayerSlot* const slot = state.players.Find(player_id)) {
            if (slot->ent_vid.has_value()) {
                if (const Ent* const ent = state.ents.GetEnt(*slot->ent_vid)) {
                    pos = ent->pos;
                }
            }
        }
        const sim::Vec2 fixed_pos = sim::ToSimVec2(pos);
        packet.player_pos_x_raw[i] = fixed_pos.x.raw_value();
        packet.player_pos_y_raw[i] = fixed_pos.y.raw_value();
    }
    packet.removed_player_count = static_cast<std::uint32_t>(std::min<std::size_t>(
        state.net_session.join_barrier_removed_player_ids.size(),
        packet.removed_player_ids.size()
    ));
    for (std::uint32_t i = 0; i < packet.removed_player_count; ++i) {
        packet.removed_player_ids[i] = state.net_session.join_barrier_removed_player_ids[i];
    }
    return packet;
}

void SendJoinBarrierTopologyPackets(State& state, NetTransportRuntime& transport) {
    if (state.net_session.join_barrier_topology_ack_peers.empty() ||
        (state.net_session.join_barrier_joined_player_ids.empty() &&
         state.net_session.join_barrier_removed_player_ids.empty())) {
        return;
    }
    const EncodedNetPacket encoded =
        EncodeJoinBarrierTopology(BuildJoinBarrierTopologyPacket(state));
    for (const PlayerId peer_id : state.net_session.join_barrier_topology_ack_peers) {
        if (state.net_session.join_barrier_transfer_id != 0 &&
            peer_id == state.net_session.join_barrier_active_peer_id) {
            continue;
        }
        const NetEndpoint* const endpoint = FindRemoteEndpointForPlayer(transport, peer_id);
        if (endpoint != nullptr) {
            SendEncodedPacket(transport, *endpoint, encoded);
        }
    }
}

bool JoinBarrierReadyToResume(const State& state) {
    return state.net_session.join_barrier_transfer_id == 0 &&
           state.net_session.join_barrier_queue.empty() &&
           state.net_session.join_barrier_topology_ack_peers.empty();
}

void StartJoinBarrierSnapshotTransfer(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    PlayerId target_peer_id
) {
    (void)graphics;
    const std::uint32_t transfer_id = state.net_session.lockstep_snapshot_resync_next_transfer_id++;
    const LockstepFrame snapshot_frame = state.net_session.lockstep_next_frame_to_step;
    const std::vector<std::uint8_t> bytes =
        SerializeSimSnapshotToBytes(MakeSimSnapshot(state));
    const std::uint32_t chunk_count = static_cast<std::uint32_t>(
        (bytes.size() + kNetSnapshotChunkPayloadBytes - 1) / kNetSnapshotChunkPayloadBytes
    );

    state.net_session.join_barrier_active_peer_id = target_peer_id;
    state.net_session.join_barrier_phase = JoinBarrierPhase::SendingSnapshot;
    state.net_session.join_barrier_transfer_id = transfer_id;
    state.net_session.join_barrier_snapshot_frame = snapshot_frame;
    state.net_session.join_barrier_chunk_count = chunk_count;
    state.net_session.join_barrier_chunks_done = 0;
    state.net_session.join_barrier_total_bytes = static_cast<std::uint32_t>(bytes.size());
    state.net_session.join_barrier_bytes_done = 0;

    state.net_session.lockstep_snapshot_resync_pending_request = false;
    state.net_session.lockstep_snapshot_resync_target_peer_id = target_peer_id;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = transfer_id;
    state.net_session.lockstep_snapshot_resync_frame = snapshot_frame;
    state.net_session.lockstep_snapshot_resync_chunk_count = chunk_count;
    state.net_session.lockstep_snapshot_resync_total_bytes =
        static_cast<std::uint32_t>(bytes.size());
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = 0;
    state.net_session.lockstep_snapshot_resync_bytes = bytes;
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = true;
    (void)transport;
    (void)endpoint;
}

void SendPendingJoinBarrier(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    if (!state.net_session.join_barrier_active) {
        return;
    }
    if (state.net_session.role != NetRole::Host) {
        return;
    }

    if (state.net_session.join_barrier_phase == JoinBarrierPhase::ReadyToResume) {
        BroadcastJoinBarrierStatus(state, transport);
        BroadcastJoinBarrierResume(state, transport);
        ClearJoinBarrier(state);
        return;
    }

    SendJoinBarrierTopologyPackets(state, transport);

    if (state.net_session.join_barrier_transfer_id == 0) {
        const std::optional<PlayerId> next_peer = PopJoinBarrierPeer(state);
        if (!next_peer.has_value()) {
            state.net_session.join_barrier_phase = JoinBarrierReadyToResume(state)
                ? JoinBarrierPhase::ReadyToResume
                : JoinBarrierPhase::WaitingForCatchup;
            BroadcastJoinBarrierStatus(state, transport);
            return;
        }

        const NetEndpoint* endpoint = FindRemoteEndpointForPlayer(transport, *next_peer);
        if (endpoint == nullptr) {
            state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
            return;
        }
        StartJoinBarrierSnapshotTransfer(state, graphics, transport, *endpoint, *next_peer);
    } else {
        const NetEndpoint* endpoint = FindRemoteEndpointForPlayer(
            transport,
            state.net_session.join_barrier_active_peer_id
        );
        if (endpoint == nullptr) {
            ClearJoinBarrierTransfer(state);
            state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
            return;
        }
        BroadcastJoinBarrierStatus(state, transport);
        SendSnapshotResyncStoredChunks(
            state,
            transport,
            *endpoint,
            kJoinBarrierChunksPerPump,
            true
        );
        if (state.net_session.join_barrier_chunks_done >=
            state.net_session.join_barrier_chunk_count) {
            state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForAck;
        }
    }

    BroadcastJoinBarrierStatus(state, transport);
}

bool HandleJoinBarrierSnapshotAck(State& state, const SnapshotResyncAckPacket& packet) {
    if (state.net_session.role != NetRole::Host ||
        !state.net_session.join_barrier_active ||
        packet.stage_instance_id != state.net_session.stage_instance_id ||
        packet.transfer_id != state.net_session.join_barrier_transfer_id) {
        return false;
    }

    ClearJoinBarrierTransfer(state);
    if (packet.success == 0) {
        state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
        QueueJoinBarrierPeer(state, static_cast<PlayerId>(packet.sender_peer_id));
        return true;
    }
    state.net_session.join_barrier_phase = JoinBarrierReadyToResume(state)
        ? JoinBarrierPhase::ReadyToResume
        : JoinBarrierPhase::WaitingForCatchup;
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

bool HasRemoteHashForFrame(const State& state, LockstepFrame frame) {
    const auto matches_frame = [frame](const LockstepRemoteHashRecord& record) {
        return record.frame == frame;
    };
    return std::any_of(
               state.net_session.lockstep_remote_hash_history.begin(),
               state.net_session.lockstep_remote_hash_history.end(),
               matches_frame
           ) ||
           std::any_of(
               state.net_session.lockstep_pending_remote_hashes.begin(),
               state.net_session.lockstep_pending_remote_hashes.end(),
               matches_frame
           );
}

bool IsScheduledLockstepHashFrame(const State& state, LockstepFrame frame) {
    if (frame == 0 || frame <= state.net_session.lockstep_hash_ignore_through_frame) {
        return false;
    }
    const LockstepFrame interval = std::max<LockstepFrame>(
        1,
        static_cast<LockstepFrame>(state.net_session.lockstep_hash_send_interval_frames)
    );
    return frame % interval == 0;
}

bool ShouldRecordCompletedLockstepHash(const State& state, LockstepFrame frame) {
    return IsScheduledLockstepHashFrame(state, frame) || HasRemoteHashForFrame(state, frame);
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

void CaptureLockstepMismatchDiagnostics(
    State& state,
    const LockstepHashRecord& local,
    const LockstepRemoteHashRecord& remote
) {
    state.net_session.lockstep_last_mismatch_peer_id = remote.peer_id;
    state.net_session.lockstep_last_mismatch_frame = remote.frame;
    state.net_session.lockstep_last_mismatch_local_hash = local.hash;
    state.net_session.lockstep_last_mismatch_remote_hash = remote.hash;
    state.net_session.lockstep_last_mismatch_local_root = local.component_root;
    state.net_session.lockstep_last_mismatch_remote_root = remote.component_root;
    state.net_session.lockstep_last_mismatch_local_stage = local.component_stage;
    state.net_session.lockstep_last_mismatch_remote_stage = remote.component_stage;
    state.net_session.lockstep_last_mismatch_local_players = local.component_players;
    state.net_session.lockstep_last_mismatch_remote_players = remote.component_players;
    state.net_session.lockstep_last_mismatch_local_tools = local.component_tools;
    state.net_session.lockstep_last_mismatch_remote_tools = remote.component_tools;
    state.net_session.lockstep_last_mismatch_local_ents = local.component_ents;
    state.net_session.lockstep_last_mismatch_remote_ents = remote.component_ents;
    state.net_session.lockstep_last_mismatch_local_ent_hashes.clear();

    if (local.component_ents != remote.component_ents) {
        const std::vector<NetworkEntFingerprint> ent_hashes =
            ComputeNetworkEntFingerprints(state);
        state.net_session.lockstep_last_mismatch_local_ent_hashes.reserve(ent_hashes.size());
        for (const NetworkEntFingerprint& ent_hash : ent_hashes) {
            state.net_session.lockstep_last_mismatch_local_ent_hashes.push_back(
                LockstepEntHashDiagnostic{
                    .net_ent_id = ent_hash.net_ent_id,
                    .type = ent_hash.type,
                    .hash = ent_hash.hash,
                }
            );
        }
    }
    DumpLockstepReplayCaptureOnDesync(state, local, remote);
}

void CompareLockstepHash(State& state, const LockstepRemoteHashRecord& remote) {
    const LockstepHashRecord* const local = FindLocalHashRecord(state, remote.frame);
    if (local == nullptr) {
        return;
    }
    RecordRemoteHashSample(state, remote);

    const bool snapshot_catchup_active =
        state.net_session.lockstep_last_desync_recovery_mode ==
            LockstepDesyncRecoveryMode::SnapshotCatchup ||
        SnapshotResyncTransferInProgress(state);
    if (snapshot_catchup_active) {
        if (local->hash == remote.hash &&
            (!state.net_session.lockstep_has_confirmed_hash ||
             remote.frame >= state.net_session.lockstep_last_confirmed_hash_frame)) {
            state.net_session.lockstep_last_confirmed_hash_frame = remote.frame;
            state.net_session.lockstep_last_confirmed_hash = local->hash;
            state.net_session.lockstep_has_confirmed_hash = true;
        }
        return;
    }

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
    CaptureLockstepMismatchDiagnostics(state, *local, remote);

    const std::optional<LockstepHashRecord> last_peer_match =
        FindLastMatchingHashWithPeer(state, remote.peer_id, remote.frame);
    if (!last_peer_match.has_value()) {
        StartOrQueueHostSnapshotResync(state, remote.peer_id);
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
        StartOrQueueHostSnapshotResync(state, remote.peer_id);
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
            CaptureLockstepMismatchDiagnostics(state, *local, remote);
            if (first_frame == 0) {
                state.net_session.lockstep_last_desync_recovery_mode =
                    LockstepDesyncRecoveryMode::FatalDesync;
            } else {
                StartOrQueueHostSnapshotResync(state, remote.peer_id);
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
        if (state.net_session.lockstep_last_desync_recovery_mode ==
            LockstepDesyncRecoveryMode::SnapshotCatchup) {
            ClearSnapshotResyncState(state);
        }
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

void RecordCompletedLockstepHash(
    State& state,
    LockstepFrame frame,
    LockstepHashContext context
) {
    if (!IsInputLockstepSession(state)) {
        return;
    }
    if (!AllRequiredInputsConfirmedForFrame(state, frame)) {
        return;
    }
    if (state.net_session.lockstep_has_recorded_hash &&
        state.net_session.lockstep_last_recorded_hash_frame == frame) {
        return;
    }

    const auto hash_start = std::chrono::steady_clock::now();
    const NetworkStateFingerprintComponents components =
        ComputeNetworkStateFingerprintComponents(state);
    const std::uint64_t hash = CombineNetworkStateFingerprintComponents(components);
    const double hash_ms = ElapsedMs(hash_start);
    state.performance_stats.lockstep_hash_ms += hash_ms;
    state.performance_stats.lockstep_hash_count_this_frame += 1;
    if (context == LockstepHashContext::Rollback) {
        state.performance_stats.lockstep_hash_rollback_ms += hash_ms;
        state.performance_stats.lockstep_hash_rollback_count_this_frame += 1;
    } else {
        state.performance_stats.lockstep_hash_normal_ms += hash_ms;
    }

    LockstepHashRecord* const existing = FindLocalHashRecord(state, frame);
    if (existing != nullptr) {
        existing->hash = hash;
        existing->component_root = components.root;
        existing->component_stage = components.stage;
        existing->component_players = components.players;
        existing->component_tools = components.tools;
        existing->component_ents = components.ents;
    } else {
        state.net_session.lockstep_hash_history.push_back(LockstepHashRecord{
            .frame = frame,
            .hash = hash,
            .component_root = components.root,
            .component_stage = components.stage,
            .component_players = components.players,
            .component_tools = components.tools,
            .component_ents = components.ents,
        });
    }
    state.net_session.lockstep_last_recorded_hash_frame = frame;
    state.net_session.lockstep_has_recorded_hash = true;
    PruneLockstepHashHistory(state);
    ProcessPendingRemoteHashes(state);
}

void RecordPreviousCompletedLockstepHash(State& state) {
    if (IsJoinBarrierBlocking(state) || IsSnapshotResyncBlocking(state)) {
        return;
    }
    if (state.net_session.lockstep_next_frame_to_step == 0) {
        return;
    }
    const LockstepFrame frame = state.net_session.lockstep_next_frame_to_step - 1;
    if (!ShouldRecordCompletedLockstepHash(state, frame)) {
        return;
    }
    RecordCompletedLockstepHash(state, frame, LockstepHashContext::Normal);
}

LockstepHashNetPacket MakeLockstepHashPacket(const State& state, const LockstepHashRecord& record) {
    LockstepHashNetPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    packet.sync_epoch = state.net_session.join_barrier_id;
    packet.frame = record.frame;
    packet.hash = record.hash;
    packet.component_root = record.component_root;
    packet.component_stage = record.component_stage;
    packet.component_players = record.component_players;
    packet.component_tools = record.component_tools;
    packet.component_ents = record.component_ents;
    return packet;
}

void SendDueLockstepHash(State& state, NetTransportRuntime& transport) {
    if (IsJoinBarrierBlocking(state) || IsSnapshotResyncBlocking(state)) {
        return;
    }
    if (!state.net_session.lockstep_has_recorded_hash) {
        return;
    }
    const LockstepFrame frame = state.net_session.lockstep_last_recorded_hash_frame;
    if (frame <= state.net_session.lockstep_hash_ignore_through_frame) {
        return;
    }
    if (!IsScheduledLockstepHashFrame(state, frame)) {
        return;
    }
    if (state.net_session.lockstep_has_sent_hash &&
        frame <= state.net_session.lockstep_last_sent_hash_frame) {
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
    snapshot.controlled_ent_vid = state.controlled_ent_vid;
    snapshot.spectator_target_player_id = state.spectator_target_player_id;
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
    state.controlled_ent_vid = snapshot.controlled_ent_vid;
    state.spectator_target_player_id = snapshot.spectator_target_player_id;
    graphics.play_cam.pos = snapshot.play_cam_pos;

    // Corrected gameplay may have changed tiles/fluids while presentation state was
    // preserved from the pre-rollback present. Rebuild caches on demand.
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
}

std::vector<PlayerId> GetConnectedPlayerIds(const State& state) {
    std::vector<PlayerId> player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.player_id != kInvalidPlayerId) {
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
            slot.player_id != kInvalidPlayerId) {
            player_ids.push_back(slot.player_id);
        }
    }
    std::sort(player_ids.begin(), player_ids.end());
    return player_ids;
}

bool AllRequiredInputsConfirmedForFrame(const State& state, LockstepFrame frame) {
    const std::vector<PlayerId> required_players = GetConnectedPlayerIds(state);
    if (state.net_session.lockstep_input_buffer.HasNonCanonicalRecordThroughFrame(
            required_players,
            frame
        )) {
        return false;
    }
    for (PlayerId player_id : required_players) {
        const LockstepInputRecord* const record =
            state.net_session.lockstep_input_buffer.FindRecord(player_id, frame);
        if (record == nullptr || record->predicted || !record->canonical) {
            return false;
        }
    }
    return true;
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

LockstepInputArbitrationStats& MutableArbitrationStatsForPlayer(
    State& state,
    PlayerId player_id
) {
    for (LockstepInputArbitrationStats& stats :
         state.net_session.lockstep_arbitration_stats_by_player) {
        if (stats.player_id == player_id) {
            return stats;
        }
    }

    state.net_session.lockstep_arbitration_stats_by_player.push_back(
        LockstepInputArbitrationStats{.player_id = player_id}
    );
    return state.net_session.lockstep_arbitration_stats_by_player.back();
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
                slot.player_id == kInvalidPlayerId) {
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
    if (record.canonical) {
        entry.input_flags |= kInputRecordFlagCanonical;
    }
    if (record.arbitrated_missing) {
        entry.input_flags |= kInputRecordFlagArbitratedMissing;
    }
    entry.mouse_x = record.input.mouse_pos.x;
    entry.mouse_y = record.input.mouse_pos.y;
    return entry;
}

LockstepInputRecord MakeLockstepInputRecord(const InputFrameRecordEntry& entry) {
    LockstepInputRecord record;
    record.player_id = entry.player_id;
    record.frame = entry.frame;
    record.sequence = entry.sequence;
    record.input = UnpackInputFrame(
        entry.input_flags & ~(kInputRecordFlagCanonical | kInputRecordFlagArbitratedMissing),
        UVec2::New(entry.mouse_x, entry.mouse_y)
    );
    record.canonical = (entry.input_flags & kInputRecordFlagCanonical) != 0U;
    record.arbitrated_missing =
        (entry.input_flags & kInputRecordFlagArbitratedMissing) != 0U;
    return record;
}

LockstepFrame FirstInputHistoryFrame(LockstepFrame last_frame) {
    constexpr LockstepFrame history_frames = kInputHistoryFrames + 1;
    if (last_frame + 1 <= history_frames) {
        return 0;
    }
    return last_frame + 1 - history_frames;
}

std::vector<InputFrameRecordsPacket> BuildLocalInputFramePackets(State& state) {
    std::vector<LockstepInputRecord> records;
    if (state.net_session.role == NetRole::Host) {
        const std::vector<PlayerId> connected_player_ids = GetConnectedPlayerIds(state);
        if (connected_player_ids.empty() || state.net_session.lockstep_next_frame_to_step == 0) {
            return {};
        }
        const LockstepFrame last_frame = state.net_session.lockstep_next_frame_to_step - 1;
        const LockstepFrame first_frame = FirstInputHistoryFrame(last_frame);
        state.net_session.lockstep_input_buffer.CollectCanonicalRecords(
            connected_player_ids,
            first_frame,
            last_frame,
            records,
            connected_player_ids.size() * static_cast<std::size_t>(kInputHistoryFrames + 1)
        );
    } else {
        const std::vector<PlayerId> local_player_ids = GetLocalPlayerIds(state);
        if (local_player_ids.empty() || state.net_session.lockstep_next_local_input_frame == 0) {
            return {};
        }

        const LockstepFrame last_frame = state.net_session.lockstep_next_local_input_frame - 1;
        const LockstepFrame first_frame = FirstInputHistoryFrame(last_frame);
        state.net_session.lockstep_input_buffer.CollectRecords(
            local_player_ids,
            first_frame,
            last_frame,
            records,
            local_player_ids.size() * static_cast<std::size_t>(kInputHistoryFrames + 1)
        );
    }

    if (records.empty()) {
        return {};
    }

    std::vector<InputFrameRecordsPacket> packets;
    packets.reserve((records.size() + kNetInputFrameRecordsPerPacket - 1) /
                    kNetInputFrameRecordsPerPacket);
    for (std::size_t offset = 0; offset < records.size();) {
        InputFrameRecordsPacket packet;
        packet.stage_instance_id = state.net_session.stage_instance_id;
        packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
        const std::size_t count =
            std::min<std::size_t>(packet.records.size(), records.size() - offset);
        packet.record_count = static_cast<std::uint32_t>(count);
        for (std::uint32_t i = 0; i < packet.record_count; ++i) {
            packet.records[i] = MakeInputFrameRecordEntry(records[offset + i]);
        }
        packets.push_back(packet);
        offset += count;
    }
    return packets;
}

void SendInputFramePacketsToEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    for (const InputFrameRecordsPacket& packet : BuildLocalInputFramePackets(state)) {
        SendEncodedPacket(transport, endpoint, EncodeInputFrameRecords(packet));
    }
}

void SendLocalInputFramePacket(State& state, NetTransportRuntime& transport) {
    if (state.net_session.role == NetRole::Host) {
        for (const NetRemoteEndpoint& remote : transport.remotes) {
            SendInputFramePacketsToEndpoint(state, transport, remote.endpoint);
        }
        return;
    }
    if (state.net_session.role == NetRole::Peer && !transport.join_request_pending) {
        SendInputFramePacketsToEndpoint(state, transport, transport.host_endpoint);
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
    LockstepFrame frame,
    const std::vector<PlayerId>& player_ids,
    const std::vector<InputFrame>& input_frames,
    bool record_replay
) {
    if (record_replay) {
        RecordLockstepReplayInputs(state, frame, player_ids, input_frames);
    }
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
    // Keep remote hash samples for the rollback window. Replayed local hashes
    // are compared against these samples to prove the rollback actually
    // repaired the divergence.
}

void SaveRollbackSnapshot(State& state, const Graphics& graphics, LockstepFrame frame) {
    (void)graphics;
    if (!state.net_session.lockstep_rollback_enabled) {
        return;
    }
    const auto save_start = std::chrono::steady_clock::now();
    for (LockstepRollbackSnapshot& entry : state.net_session.lockstep_rollback_snapshots) {
        if (entry.frame == frame) {
            entry.snapshot = std::make_shared<SimSnapshot>(MakeSimSnapshot(state));
            state.performance_stats.rollback_snapshot_save_ms += ElapsedMs(save_start);
            return;
        }
    }
    state.net_session.lockstep_rollback_snapshots.push_back(LockstepRollbackSnapshot{
        .frame = frame,
        .snapshot = std::make_shared<SimSnapshot>(MakeSimSnapshot(state)),
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
                return entry.frame < first_kept;
            }
        ),
        snapshots.end()
    );
}

const SimSnapshot* FindRollbackSnapshot(const State& state, LockstepFrame frame) {
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

void StoreHostCanonicalInput(
    State& state,
    PlayerId player_id,
    LockstepFrame frame,
    InputFrame input,
    bool arbitrated_missing = false
) {
    LockstepInputRecord canonical;
    canonical.player_id = player_id;
    canonical.frame = frame;
    canonical.sequence = state.net_session.lockstep_next_input_sequence++;
    canonical.input = input;
    canonical.predicted = false;
    canonical.canonical = true;
    canonical.arbitrated_missing = arbitrated_missing;
    if (!arbitrated_missing) {
        MutableArbitrationStatsForPlayer(state, player_id).last_missing_span = 0;
    }
    const LockstepInputStoreResult result =
        state.net_session.lockstep_input_buffer.Store(canonical);
    if (result.mismatch_frame.has_value() &&
        frame < state.net_session.lockstep_next_frame_to_step) {
        RequestRollbackFromFrame(state, *result.mismatch_frame);
    }
}

void SeedLockstepInputBaselineForPlayers(
    State& state,
    const std::vector<PlayerId>& player_ids,
    LockstepFrame barrier_frame
) {
    if (player_ids.empty()) {
        return;
    }

    LockstepFrame baseline_frame = barrier_frame > 0 ? barrier_frame - 1 : 0;
    baseline_frame = std::max<LockstepFrame>(
        baseline_frame,
        static_cast<LockstepFrame>(state.net_session.lockstep_input_delay_frames)
    );

    for (PlayerId player_id : player_ids) {
        if (player_id == kInvalidPlayerId) {
            continue;
        }
        const std::optional<LockstepFrame> latest =
            state.net_session.lockstep_input_buffer.LatestFrameForPlayer(
                player_id,
                false
            );
        if (latest.has_value() && *latest >= baseline_frame) {
            continue;
        }

        LockstepInputRecord baseline;
        baseline.player_id = player_id;
        baseline.frame = baseline_frame;
        baseline.sequence = state.net_session.lockstep_next_input_sequence++;
        baseline.input = InputFrame::New();
        baseline.predicted = false;
        baseline.canonical = false;
        (void)state.net_session.lockstep_input_buffer.Store(baseline);
    }
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
        if (state.net_session.role == NetRole::Host) {
            if (input == nullptr) {
                out_inputs.clear();
                return false;
            } else {
                const LockstepInputRecord* const record =
                    state.net_session.lockstep_input_buffer.FindRecord(player_id, frame);
                if (record != nullptr && !record->canonical) {
                    StoreHostCanonicalInput(state, player_id, frame, *input);
                    input = state.net_session.lockstep_input_buffer.Find(player_id, frame);
                }
            }
        }
        if (input == nullptr && allow_prediction &&
            std::find(local_player_ids.begin(), local_player_ids.end(), player_id) ==
                local_player_ids.end()) {
            LockstepInputRecord prediction;
            prediction.player_id = player_id;
            prediction.frame = frame;
            prediction.input = PredictRemoteInputForFrame(state, player_id, frame);
            prediction.predicted = true;
            prediction.canonical = false;
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

    const SimSnapshot* const snapshot = FindRollbackSnapshot(state, rollback_frame);
    if (snapshot == nullptr) {
        StartOrQueueHostSnapshotResync(state, state.net_session.lockstep_last_mismatch_peer_id);
        return false;
    }

    const auto replay_start = std::chrono::steady_clock::now();
    const RollbackPresentationSnapshot presentation_snapshot =
        CaptureRollbackPresentationState(state, graphics);
    Audio replay_audio;
    const auto restore_start = std::chrono::steady_clock::now();
    RestoreSimSnapshot(*snapshot, state, graphics);
    RelinkPlayerNetEnts(state);
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
        ApplyLockstepInputsToState(state, frame, required_players, frame_inputs, false);
        state.net_session.lockstep_next_frame_to_step += 1;
        StepSingleTickWithMode(state, replay_audio, graphics, SimulationTickMode::ReplayNoNetwork);
        if (ShouldRecordCompletedLockstepHash(state, frame)) {
            RecordCompletedLockstepHash(state, frame, LockstepHashContext::Rollback);
        }
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

void PumpInputLockstepPackets(State& state, Graphics& graphics, NetTransportRuntime& transport) {
    const auto pump_start = std::chrono::steady_clock::now();
    transport.pump_tick += 1U;
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

void RequestHostSnapshotResync(State& state, PlayerId target_peer_id) {
    StartOrQueueHostSnapshotResync(state, target_peer_id);
}

void BeginJoinBarrierCatchup(State& state, PlayerId target_peer_id) {
    if (state.net_session.role != NetRole::Host || target_peer_id == kInvalidPlayerId) {
        return;
    }
    if (!state.net_session.join_barrier_active) {
        state.net_session.join_barrier_active = true;
        state.net_session.join_barrier_id += 1U;
        if (state.net_session.join_barrier_id == 0) {
            state.net_session.join_barrier_id = 1;
        }
        state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
        state.net_session.join_barrier_queue.clear();
        ClearJoinBarrierTransfer(state);
    }
    QueueJoinBarrierPeer(state, target_peer_id);
}

void BeginJoinBarrierTopologyChange(
    State& state,
    const NetTransportRuntime& transport,
    const std::vector<PlayerId>& joined_player_ids
) {
    if (state.net_session.role != NetRole::Host || joined_player_ids.empty()) {
        return;
    }
    const std::vector<PlayerId> endpoint_targets =
        JoinBarrierEndpointTargetsForPlayers(transport, joined_player_ids);
    if (endpoint_targets.empty()) {
        return;
    }
    if (!state.net_session.join_barrier_active) {
        BeginJoinBarrierCatchup(state, endpoint_targets.front());
    }

    std::vector<PlayerId>& joined = state.net_session.join_barrier_joined_player_ids;
    for (const PlayerId player_id : joined_player_ids) {
        if (player_id == kInvalidPlayerId) {
            continue;
        }
        if (std::find(joined.begin(), joined.end(), player_id) == joined.end()) {
            joined.push_back(player_id);
        }
    }
    SortUniqueValidPlayerIds(joined);
    for (const PlayerId target_peer_id : endpoint_targets) {
        QueueJoinBarrierPeer(state, target_peer_id);
    }

    // If another peer joins while a catchup snapshot is already in flight, the
    // active peer's snapshot is stale only for topology. Finish its snapshot,
    // then send a topology delta instead of forcing a second full snapshot.
    if (state.net_session.join_barrier_active_peer_id != kInvalidPlayerId) {
        QueueJoinBarrierTopologyAck(state, state.net_session.join_barrier_active_peer_id);
    }

    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (remote.player_ids.empty()) {
            continue;
        }
        const PlayerId peer_id = remote.player_ids.front();
        bool endpoint_is_joining = false;
        for (const PlayerId player_id : remote.player_ids) {
            if (std::find(joined.begin(), joined.end(), player_id) != joined.end() ||
                std::find(
                    state.net_session.join_barrier_queue.begin(),
                    state.net_session.join_barrier_queue.end(),
                    player_id
                ) != state.net_session.join_barrier_queue.end() ||
                player_id == state.net_session.join_barrier_active_peer_id) {
                endpoint_is_joining = true;
                break;
            }
        }
        if (!endpoint_is_joining) {
            QueueJoinBarrierTopologyAck(state, peer_id);
        }
    }
}

void BeginJoinBarrierTopologyRemoval(
    State& state,
    const NetTransportRuntime& transport,
    const std::vector<PlayerId>& removed_player_ids
) {
    if (state.net_session.role != NetRole::Host || removed_player_ids.empty()) {
        return;
    }
    if (!state.net_session.join_barrier_active) {
        state.net_session.join_barrier_active = true;
        state.net_session.join_barrier_id += 1U;
        if (state.net_session.join_barrier_id == 0) {
            state.net_session.join_barrier_id = 1;
        }
        state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
        state.net_session.join_barrier_queue.clear();
        ClearJoinBarrierTransfer(state);
    }

    std::vector<PlayerId>& removed = state.net_session.join_barrier_removed_player_ids;
    for (const PlayerId player_id : removed_player_ids) {
        if (player_id == kInvalidPlayerId ||
            player_id == state.net_session.local_player_id) {
            continue;
        }
        if (std::find(removed.begin(), removed.end(), player_id) == removed.end()) {
            removed.push_back(player_id);
        }
    }
    SortUniqueValidPlayerIds(removed);

    auto erase_removed = [&](std::vector<PlayerId>& values) {
        values.erase(
            std::remove_if(
                values.begin(),
                values.end(),
                [&](PlayerId player_id) {
                    return std::find(
                        removed_player_ids.begin(),
                        removed_player_ids.end(),
                        player_id
                    ) != removed_player_ids.end();
                }
            ),
            values.end()
        );
    };
    erase_removed(state.net_session.join_barrier_joined_player_ids);
    erase_removed(state.net_session.join_barrier_queue);
    erase_removed(state.net_session.join_barrier_topology_ack_peers);
    SortUniqueValidPlayerIds(state.net_session.join_barrier_joined_player_ids);
    SortUniqueValidPlayerIds(state.net_session.join_barrier_topology_ack_peers);
    if (std::find(
            removed_player_ids.begin(),
            removed_player_ids.end(),
            state.net_session.join_barrier_active_peer_id
        ) != removed_player_ids.end()) {
        ClearJoinBarrierTransfer(state);
    }

    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (remote.player_ids.empty()) {
            continue;
        }
        QueueJoinBarrierTopologyAck(state, remote.player_ids.front());
    }
}

void HandleJoinBarrierStatus(State& state, const JoinBarrierStatusPacket& packet) {
    if (state.net_session.role != NetRole::Peer ||
        !PacketMatchesCurrentOrAcceptedNextStage(state, packet.stage_instance_id) ||
        packet.barrier_id < state.net_session.join_barrier_id) {
        return;
    }

    if (packet.active == 0) {
        if (packet.barrier_id >= state.net_session.join_barrier_id) {
            ClearJoinBarrier(state);
            state.net_session.join_barrier_id = packet.barrier_id;
        }
        return;
    }

    state.net_session.join_barrier_active = true;
    state.net_session.join_barrier_id = packet.barrier_id;
    state.net_session.join_barrier_phase =
        static_cast<JoinBarrierPhase>(packet.phase);
    state.net_session.join_barrier_active_peer_id = packet.active_player_id;
    state.net_session.join_barrier_queue.clear();
    for (std::uint32_t i = 0; i < packet.queued_peer_count; ++i) {
        state.net_session.join_barrier_queue.push_back(packet.queued_peer_ids[i]);
    }
    state.net_session.join_barrier_transfer_id = packet.transfer_id;
    state.net_session.join_barrier_snapshot_frame = packet.snapshot_frame;
    state.net_session.join_barrier_chunk_count = packet.chunk_count;
    state.net_session.join_barrier_chunks_done = packet.chunks_done;
    state.net_session.join_barrier_total_bytes = packet.total_bytes;
    state.net_session.join_barrier_bytes_done = packet.bytes_done;
}

void SendJoinBarrierTopologyAck(
    State& state,
    NetTransportRuntime& transport,
    std::uint32_t barrier_id,
    std::uint8_t success
) {
    JoinBarrierTopologyAckPacket ack;
    ack.stage_instance_id = state.net_session.stage_instance_id;
    ack.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
    ack.barrier_id = barrier_id;
    ack.success = success;
    SendEncodedPacket(transport, transport.host_endpoint, EncodeJoinBarrierTopologyAck(ack));
}

bool RestoreTopologyBarrierFrame(
    State& state,
    Graphics& graphics,
    LockstepFrame barrier_frame
) {
    if (state.net_session.lockstep_next_frame_to_step <= barrier_frame) {
        return true;
    }

    const SimSnapshot* const snapshot = FindRollbackSnapshot(state, barrier_frame);
    if (snapshot == nullptr) {
        return false;
    }

    RestoreSimSnapshot(*snapshot, state, graphics);
    return true;
}

void HandleJoinBarrierTopology(
    State& state,
    Graphics& graphics,
    NetTransportRuntime& transport,
    const JoinBarrierTopologyPacket& packet
) {
    if (state.net_session.role != NetRole::Peer ||
        !PacketMatchesCurrentOrAcceptedNextStage(state, packet.stage_instance_id) ||
        packet.barrier_id < state.net_session.join_barrier_id) {
        return;
    }

    if (!RestoreTopologyBarrierFrame(state, graphics, packet.barrier_frame)) {
        SendJoinBarrierTopologyAck(state, transport, packet.barrier_id, 0);
        return;
    }

    state.net_session.join_barrier_active = true;
    state.net_session.join_barrier_id = packet.barrier_id;
    state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForResume;
    state.net_session.lockstep_next_frame_to_step = packet.barrier_frame;
    state.net_session.lockstep_next_local_input_frame = std::max<LockstepFrame>(
        state.net_session.lockstep_next_local_input_frame,
        packet.barrier_frame
    );
    state.net_session.lockstep_rollback_snapshots.clear();
    state.net_session.lockstep_rollback_requested_frame = std::nullopt;
    state.net_session.join_barrier_joined_player_ids.clear();
    state.net_session.join_barrier_removed_player_ids.clear();
    ClearLockstepHashState(state);
    SetPostCatchupHashQuietWindow(state, packet.barrier_frame);

    std::vector<PlayerId> removed_player_ids;
    removed_player_ids.reserve(packet.removed_player_count);
    for (std::uint32_t i = 0; i < packet.removed_player_count; ++i) {
        if (packet.removed_player_ids[i] != kInvalidPlayerId) {
            removed_player_ids.push_back(packet.removed_player_ids[i]);
            state.net_session.join_barrier_removed_player_ids.push_back(
                packet.removed_player_ids[i]
            );
        }
    }
    SortUniqueValidPlayerIds(removed_player_ids);
    SortUniqueValidPlayerIds(state.net_session.join_barrier_removed_player_ids);
    if (!removed_player_ids.empty()) {
        RemoveRemotePlayers(state, transport, removed_player_ids);
    }

    for (std::uint32_t i = 0; i < packet.player_count; ++i) {
        const PlayerId player_id = packet.player_ids[i];
        if (player_id == kInvalidPlayerId) {
            continue;
        }
        if (const PlayerSlot* const existing = state.players.Find(player_id)) {
            if (existing->connection_kind == PlayerConnectionKind::Local) {
                continue;
            }
        }
        state.net_session.join_barrier_joined_player_ids.push_back(player_id);
        EnsureSpawnedPlayer(
            state,
            player_id,
            false,
            false,
            sim::ToRenderVec2(
                sim::Vec2::from_raw(packet.player_pos_x_raw[i], packet.player_pos_y_raw[i])
            ),
            graphics
        );
    }
    SortUniqueValidPlayerIds(state.net_session.join_barrier_joined_player_ids);

    SeedLockstepInputBaselineForPlayers(
        state,
        state.net_session.join_barrier_joined_player_ids,
        packet.barrier_frame
    );

    SendJoinBarrierTopologyAck(state, transport, packet.barrier_id, 1);
}

void HandleJoinBarrierTopologyAck(State& state, const JoinBarrierTopologyAckPacket& packet) {
    if (state.net_session.role != NetRole::Host ||
        !state.net_session.join_barrier_active ||
        packet.stage_instance_id != state.net_session.stage_instance_id ||
        packet.barrier_id != state.net_session.join_barrier_id) {
        return;
    }
    const PlayerId peer_id = static_cast<PlayerId>(packet.sender_peer_id);
    std::vector<PlayerId>& peers = state.net_session.join_barrier_topology_ack_peers;
    peers.erase(std::remove(peers.begin(), peers.end(), peer_id), peers.end());
    if (packet.success == 0) {
        QueueJoinBarrierPeer(state, peer_id);
        state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
        return;
    }
    if (JoinBarrierReadyToResume(state)) {
        state.net_session.join_barrier_phase = JoinBarrierPhase::ReadyToResume;
    }
}

void HandleJoinBarrierResume(State& state, const JoinBarrierResumePacket& packet) {
    if (state.net_session.role != NetRole::Peer ||
        !PacketMatchesCurrentOrAcceptedNextStage(state, packet.stage_instance_id) ||
        packet.barrier_id < state.net_session.join_barrier_id) {
        return;
    }
    state.net_session.stage_instance_id = packet.stage_instance_id;
    state.net_session.lockstep_next_frame_to_step = packet.resume_frame;
    state.net_session.lockstep_next_local_input_frame = packet.resume_frame;
    state.net_session.join_barrier_id = packet.barrier_id;
    ClearJoinBarrier(state);
}

void HandleSnapshotResyncRequest(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const SnapshotResyncRequestPacket& packet
) {
    if (state.net_session.role != NetRole::Host ||
        packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    const PlayerId target_peer_id = static_cast<PlayerId>(packet.sender_peer_id);
    if (SnapshotResyncTransferInProgress(state)) {
        QueueSnapshotResyncTarget(state, target_peer_id);
        return;
    }
    SendSnapshotResyncChunksToEndpoint(
        state,
        graphics,
        transport,
        endpoint,
        target_peer_id
    );
}

void RelinkPlayerNetEnts(State& state) {
    state.net_session.ent_links.erase(
        std::remove_if(
            state.net_session.ent_links.begin(),
            state.net_session.ent_links.end(),
            [](const NetEntLink& link) {
                return IsPlayerNetEntId(link.net_id);
            }
        ),
        state.net_session.ent_links.end()
    );

    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.player_id == kInvalidPlayerId || !slot.ent_vid.has_value()) {
            continue;
        }

        const Ent* const ent = state.ents.GetEnt(*slot.ent_vid);
        if (ent == nullptr || !ent->active) {
            continue;
        }
        state.net_session.LinkEnt(MakePlayerNetEntId(slot.player_id), *slot.ent_vid);
    }
}

void HandleSnapshotResyncChunk(
    State& state,
    Graphics& graphics,
    NetTransportRuntime& transport,
    const SnapshotResyncChunkPacket& packet
) {
    if (state.net_session.role != NetRole::Peer ||
        !PacketMatchesCurrentOrAcceptedNextStage(state, packet.stage_instance_id) ||
        packet.chunk_count == 0 ||
        packet.chunk_index >= packet.chunk_count ||
        packet.total_bytes == 0) {
        return;
    }

    const bool join_barrier_chunk =
        state.net_session.join_barrier_active &&
        (state.net_session.join_barrier_transfer_id == 0 ||
         state.net_session.join_barrier_transfer_id == packet.transfer_id);

    if (state.net_session.lockstep_snapshot_resync_last_acked_transfer_id ==
            packet.transfer_id &&
        state.net_session.lockstep_snapshot_resync_last_acked_frame ==
            packet.snapshot_frame) {
        SendSnapshotResyncAck(
            state,
            transport,
            packet.transfer_id,
            packet.snapshot_frame,
            state.net_session.lockstep_snapshot_resync_last_ack_success != 0
        );
        return;
    }

    if (state.net_session.lockstep_snapshot_resync_active_transfer_id != packet.transfer_id) {
        state.net_session.lockstep_snapshot_resync_active_transfer_id = packet.transfer_id;
        state.net_session.lockstep_snapshot_resync_frame = packet.snapshot_frame;
        state.net_session.lockstep_snapshot_resync_chunk_count = packet.chunk_count;
        state.net_session.lockstep_snapshot_resync_total_bytes = packet.total_bytes;
        state.net_session.lockstep_snapshot_resync_bytes.assign(packet.total_bytes, 0);
        state.net_session.lockstep_snapshot_resync_received_chunks.assign(packet.chunk_count, 0);
        if (join_barrier_chunk) {
            state.net_session.join_barrier_transfer_id = packet.transfer_id;
            state.net_session.join_barrier_snapshot_frame = packet.snapshot_frame;
            state.net_session.join_barrier_chunk_count = packet.chunk_count;
            state.net_session.join_barrier_total_bytes = packet.total_bytes;
            state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
        } else {
            state.net_session.lockstep_last_desync_recovery_mode =
                LockstepDesyncRecoveryMode::SnapshotCatchup;
        }
    }

    if (state.net_session.lockstep_snapshot_resync_bytes.size() != packet.total_bytes ||
        state.net_session.lockstep_snapshot_resync_received_chunks.size() != packet.chunk_count) {
        return;
    }

    const std::size_t begin =
        static_cast<std::size_t>(packet.chunk_index) * kNetSnapshotChunkPayloadBytes;
    if (begin + packet.payload_bytes > state.net_session.lockstep_snapshot_resync_bytes.size()) {
        return;
    }
    std::copy_n(
        packet.payload.begin(),
        packet.payload_bytes,
        state.net_session.lockstep_snapshot_resync_bytes.begin() + static_cast<std::ptrdiff_t>(begin)
    );
    state.net_session.lockstep_snapshot_resync_received_chunks[packet.chunk_index] = 1;
    if (join_barrier_chunk) {
        const auto received_count = static_cast<std::uint32_t>(std::count_if(
            state.net_session.lockstep_snapshot_resync_received_chunks.begin(),
            state.net_session.lockstep_snapshot_resync_received_chunks.end(),
            [](std::uint8_t received) { return received != 0; }
        ));
        state.net_session.join_barrier_chunks_done = received_count;
        const std::size_t done_bytes = std::min<std::size_t>(
            state.net_session.lockstep_snapshot_resync_bytes.size(),
            static_cast<std::size_t>(received_count) * kNetSnapshotChunkPayloadBytes
        );
        state.net_session.join_barrier_bytes_done = static_cast<std::uint32_t>(done_bytes);
    }

    const bool complete = std::all_of(
        state.net_session.lockstep_snapshot_resync_received_chunks.begin(),
        state.net_session.lockstep_snapshot_resync_received_chunks.end(),
        [](std::uint8_t received) { return received != 0; }
    );
    if (!complete) {
        return;
    }

    SimSnapshot snapshot;
    const bool decoded = DeserializeSimSnapshotFromBytes(
        state.net_session.lockstep_snapshot_resync_bytes,
        snapshot
    );
    if (decoded) {
        const std::vector<StageTileTrigger> local_tile_triggers = state.stage.tile_triggers;
        RestoreSimSnapshot(snapshot, state, graphics);
        state.stage.tile_triggers = local_tile_triggers;
        for (Ent& ent : state.ents.ents) {
            RestoreEntRuntimeCallbacksFromSpec(ent);
        }
        RelinkPlayerNetEnts(state);
        state.net_session.stage_instance_id = packet.stage_instance_id;
        state.net_session.quest_id = state.stage.quest_id;
        state.net_session.quest_stage_id = state.stage.quest_stage_id;
        state.net_session.stage_seed = state.stage.generation_seed.value_or(state.net_session.stage_seed);
        NotifyRunRestartStageLoaded(state);
        state.net_session.lockstep_next_frame_to_step = packet.snapshot_frame;
        state.net_session.lockstep_next_local_input_frame = packet.snapshot_frame;
        state.net_session.lockstep_input_buffer = LockstepInputBuffer{};
        ClearLockstepHashState(state);
        state.net_session.lockstep_rollback_snapshots.clear();
        state.net_session.lockstep_rollback_requested_frame = std::nullopt;
        state.net_session.lockstep_last_desync_recovery_mode =
            join_barrier_chunk
                ? LockstepDesyncRecoveryMode::None
                : LockstepDesyncRecoveryMode::RollbackRepaired;
        if (join_barrier_chunk) {
            state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForResume;
        }
        InvalidateStageLighting(state);
        InvalidateStageAcoustics(state);
    } else {
        state.net_session.lockstep_last_desync_recovery_mode =
            LockstepDesyncRecoveryMode::FatalDesync;
    }

    SendSnapshotResyncAck(state, transport, packet.transfer_id, packet.snapshot_frame, decoded);
    state.net_session.lockstep_snapshot_resync_bytes.clear();
    state.net_session.lockstep_snapshot_resync_received_chunks.clear();
    state.net_session.lockstep_snapshot_resync_active_transfer_id = 0;
    state.net_session.lockstep_snapshot_resync_chunk_count = 0;
    state.net_session.lockstep_snapshot_resync_total_bytes = 0;
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = 0;
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    if (join_barrier_chunk) {
        state.net_session.join_barrier_chunks_done = state.net_session.join_barrier_chunk_count;
        state.net_session.join_barrier_bytes_done = state.net_session.join_barrier_total_bytes;
    }
}

void HandleSnapshotResyncAck(State& state, const SnapshotResyncAckPacket& packet) {
    if (HandleJoinBarrierSnapshotAck(state, packet)) {
        return;
    }
    if (state.net_session.role != NetRole::Host ||
        packet.stage_instance_id != state.net_session.stage_instance_id ||
        packet.transfer_id != state.net_session.lockstep_snapshot_resync_active_transfer_id) {
        return;
    }
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = false;
    state.net_session.lockstep_snapshot_resync_pending_request = false;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = 0;
    state.net_session.lockstep_snapshot_resync_target_peer_id = kInvalidPlayerId;
    state.net_session.lockstep_snapshot_resync_chunk_count = 0;
    state.net_session.lockstep_snapshot_resync_total_bytes = 0;
    state.net_session.lockstep_snapshot_resync_next_chunk_to_send = 0;
    state.net_session.lockstep_snapshot_resync_bytes.clear();
    state.net_session.lockstep_snapshot_resync_received_chunks.clear();
    state.net_session.lockstep_snapshot_resync_retry_ticks = 0;
    if (packet.success == 0) {
        state.net_session.lockstep_last_desync_recovery_mode =
            LockstepDesyncRecoveryMode::FatalDesync;
        state.net_session.lockstep_snapshot_resync_queue.clear();
        return;
    }
    if (const std::optional<PlayerId> next_target = PopSnapshotResyncTarget(state)) {
        StartOrQueueHostSnapshotResync(state, *next_target);
        return;
    }
    state.net_session.lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::RollbackRepaired;
}

bool IsInputLockstepActive(const State& state) {
    return IsInputLockstepSession(state) &&
           state.net_transport != nullptr &&
           state.net_transport->socket.IsOpen();
}

bool IsInputLockstepSession(const State& state) {
    return state.net_session.role != NetRole::Offline &&
           state.net_session.input_lockstep_enabled;
}

bool IsInputLockstepCatchupBlocking(const State& state) {
    return IsInputLockstepSession(state) &&
           (IsJoinBarrierBlocking(state) || IsSnapshotResyncBlocking(state));
}

bool ForceLockstepSnapshotResync(
    State& state,
    PlayerId target_player_id,
    std::string* status_out
) {
    if (!IsInputLockstepSession(state)) {
        if (status_out != nullptr) {
            *status_out = "No active input-lockstep session.";
        }
        return false;
    }

    if (state.net_session.role == NetRole::Host) {
        if (target_player_id == kInvalidPlayerId) {
            for (const NetPeerState& peer : state.net_session.peers) {
                if (peer.connected && peer.player_id != state.net_session.local_player_id) {
                    target_player_id = peer.player_id;
                    break;
                }
            }
        }
        if (target_player_id == kInvalidPlayerId ||
            target_player_id == state.net_session.local_player_id) {
            if (status_out != nullptr) {
                *status_out = "No connected remote player to resync.";
            }
            return false;
        }

        const auto peer_it = std::find_if(
            state.net_session.peers.begin(),
            state.net_session.peers.end(),
            [target_player_id](const NetPeerState& peer) {
                return peer.connected && peer.player_id == target_player_id;
            }
        );
        if (peer_it == state.net_session.peers.end()) {
            if (status_out != nullptr) {
                *status_out = "Target player is not a connected remote peer.";
            }
            return false;
        }

        StartOrQueueHostSnapshotResync(state, target_player_id);
        if (status_out != nullptr) {
            *status_out = "Queued host snapshot resync for player " +
                std::to_string(target_player_id) + ".";
        }
        return true;
    }

    if (state.net_session.role == NetRole::Peer) {
        StartOrQueueHostSnapshotResync(state, state.net_session.local_player_id);
        if (status_out != nullptr) {
            *status_out = "Queued host snapshot resync request.";
        }
        return true;
    }

    if (status_out != nullptr) {
        *status_out = "Unsupported network role for snapshot resync.";
    }
    return false;
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
    ResetLockstepReplayCapture(state);
    state.net_session.lockstep_last_recorded_hash_frame = 0;
    state.net_session.lockstep_has_recorded_hash = false;
    state.net_session.lockstep_last_sent_hash_frame = 0;
    state.net_session.lockstep_has_sent_hash = false;
    state.net_session.lockstep_hash_ignore_through_frame = 0;
    state.net_session.lockstep_hash_mismatch_count = 0;
    state.net_session.lockstep_last_mismatch_peer_id = kInvalidPlayerId;
    state.net_session.lockstep_last_mismatch_frame = 0;
    state.net_session.lockstep_last_mismatch_local_hash = 0;
    state.net_session.lockstep_last_mismatch_remote_hash = 0;
    state.net_session.lockstep_last_mismatch_local_root = 0;
    state.net_session.lockstep_last_mismatch_remote_root = 0;
    state.net_session.lockstep_last_mismatch_local_stage = 0;
    state.net_session.lockstep_last_mismatch_remote_stage = 0;
    state.net_session.lockstep_last_mismatch_local_players = 0;
    state.net_session.lockstep_last_mismatch_remote_players = 0;
    state.net_session.lockstep_last_mismatch_local_tools = 0;
    state.net_session.lockstep_last_mismatch_remote_tools = 0;
    state.net_session.lockstep_last_mismatch_local_ents = 0;
    state.net_session.lockstep_last_mismatch_remote_ents = 0;
    state.net_session.lockstep_last_mismatch_local_ent_hashes.clear();
    state.net_session.lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::None;
    ClearSnapshotResyncState(state);
    state.net_session.lockstep_snapshot_resync_last_acked_transfer_id = 0;
    state.net_session.lockstep_snapshot_resync_last_acked_frame = 0;
    state.net_session.lockstep_snapshot_resync_last_ack_success = 0;
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
    state.net_session.lockstep_arbitrated_missing_input_count = 0;
    state.net_session.lockstep_arbitrated_neutral_input_count = 0;
    state.net_session.lockstep_last_arbitrated_missing_span = 0;
    state.net_session.lockstep_arbitration_stats_by_player.clear();
    ClearJoinBarrierFields(state);
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

    if (IsJoinBarrierBlocking(state)) {
        return false;
    }
    if (HasFatalLockstepDesync(state)) {
        return false;
    }
    if (IsSnapshotResyncBlocking(state)) {
        return false;
    }
    if (state.net_session.role == NetRole::Peer &&
        state.net_transport &&
        state.net_transport->join_request_pending) {
        return false;
    }

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
        state.net_session.lockstep_input_wait_block_count += 1;
        return false;
    }

    SaveRollbackSnapshot(state, graphics, state.net_session.lockstep_next_frame_to_step);
    state.performance_stats.rollback_buffer_bytes = ApproxRollbackBufferBytes(state);
    ApplyLockstepInputsToState(
        state,
        state.net_session.lockstep_next_frame_to_step,
        required_players,
        frame_inputs,
        true
    );
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

void MaintainInputLockstepTransport(State& state, Graphics& graphics) {
    if (!IsInputLockstepActive(state)) {
        return;
    }

    NetTransportRuntime& transport = *state.net_transport;
    transport.fuzzer_config = state.net_session.fuzzer_config;
    MaintainRealnetPunch(state, transport);
    MaintainRealnetRelay(state, transport);
    PumpInputLockstepPackets(state, graphics, transport);
    SendPendingRunRestart(state, transport);
    if (ApplyDueRunRestart(state)) {
        FlushFuzzedOutgoingPackets(transport);
        state.net_session.fuzzer_stats = transport.fuzzer_stats;
        return;
    }
    SendPendingJoinBarrier(state, graphics, transport);
    SendPendingSnapshotResync(state, graphics, transport);
    if (HasFatalLockstepDesync(state)) {
        FlushFuzzedOutgoingPackets(transport);
        state.net_session.fuzzer_stats = transport.fuzzer_stats;
        return;
    }
    ApplyDueLockstepSettings(state);
    UpdateLockstepAutoDelay(state);
    SendBroadcastLockstepSettings(state, transport);
    QueueLocalInputsThroughTargetFrame(state);
    SendLocalInputFramePacket(state, transport);
    RecordPreviousCompletedLockstepHash(state);
    SendDueLockstepHash(state, transport);
    FlushFuzzedOutgoingPackets(transport);
    state.net_session.fuzzer_stats = transport.fuzzer_stats;
}

void HandleInputFrameRecords(State& state, const InputFrameRecordsPacket& packet) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    if (state.net_session.role == NetRole::Peer &&
        static_cast<PlayerId>(packet.sender_peer_id) != state.net_session.host_player_id) {
        return;
    }
    for (std::uint32_t i = 0; i < packet.record_count; ++i) {
        LockstepInputRecord record = MakeLockstepInputRecord(packet.records[i]);
        if (state.net_session.role == NetRole::Host && record.canonical) {
            continue;
        }
        if (state.net_session.role == NetRole::Peer && !record.canonical) {
            continue;
        }
        if (state.net_session.role == NetRole::Host && !record.canonical) {
            MutableArbitrationStatsForPlayer(state, record.player_id).last_missing_span = 0;
        }
        const LockstepInputStoreResult result =
            state.net_session.lockstep_input_buffer.Store(record);
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
    if (packet.sync_epoch != state.net_session.join_barrier_id) {
        return;
    }
    if (packet.frame <= state.net_session.lockstep_hash_ignore_through_frame) {
        return;
    }
    if (IsJoinBarrierBlocking(state) || IsSnapshotResyncBlocking(state)) {
        return;
    }
    StoreOrCompareRemoteHash(state, LockstepRemoteHashRecord{
        .peer_id = static_cast<PlayerId>(packet.sender_peer_id),
        .frame = packet.frame,
        .hash = packet.hash,
        .component_root = packet.component_root,
        .component_stage = packet.component_stage,
        .component_players = packet.component_players,
        .component_tools = packet.component_tools,
        .component_ents = packet.component_ents,
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

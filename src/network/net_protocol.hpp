#pragma once

#include "network/lockstep_config.hpp"
#include "network/net_ids.hpp"
#include "network/net_limits.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace splonks::network {

constexpr std::uint32_t kNetProtocolMagic = 0x534C504B; // SLPK
constexpr std::uint16_t kNetProtocolVersion = 3;
constexpr std::size_t kNetNameBytes = 32;
constexpr std::size_t kNetQuestIdBytes = 32;
constexpr std::size_t kNetQuestStageIdBytes = 64;
constexpr std::size_t kNetPlayersPerProcess = 16;
constexpr std::size_t kNetInputFrameRecordsPerPacket = 16;
constexpr std::size_t kNetSnapshotChunkPayloadBytes = 480;

enum class NetPacketType : std::uint16_t {
    JoinRequest = 1,
    JoinAccept = 2,
    Ping = 3,
    Pong = 4,
    JoinPending = 5,
    LeaveNotice = 9,
    InputFrameRecords = 20,
    LockstepSettings = 21,
    LockstepHash = 22,
    SnapshotResyncRequest = 23,
    SnapshotResyncChunk = 24,
    SnapshotResyncAck = 25,
    JoinBarrierStatus = 26,
    JoinBarrierResume = 27,
    JoinBarrierTopology = 28,
    JoinBarrierTopologyAck = 29,
    RunRestart = 30,
};

enum class JoinPendingReason : std::uint8_t {
    None = 0,
    StageTransition = 1,
    ContentMismatch = 2,
};

struct NetPacketHeader {
    std::uint32_t magic = kNetProtocolMagic;
    std::uint16_t version = kNetProtocolVersion;
    NetPacketType type = NetPacketType::JoinRequest;
    std::uint16_t payload_bytes = 0;
};

struct JoinRequestPacket {
    std::uint32_t local_player_count = 1;
    std::uint32_t preferred_player_count = 0;
    std::array<PlayerId, kNetPlayersPerProcess> preferred_player_ids{};
    std::uint64_t content_hash = 0;
    std::array<char, kNetNameBytes> display_name{};
};

struct JoinAcceptPacket {
    std::uint32_t assigned_player_count = 1;
    std::array<PlayerId, kNetPlayersPerProcess> assigned_player_ids{};
    PlayerId host_player_id = kPrimaryLocalPlayerId;
    StageInstanceId stage_instance_id = 1;
    float remote_spawn_x = 0.0F;
    float remote_spawn_y = 0.0F;
    float host_spawn_x = 0.0F;
    float host_spawn_y = 0.0F;
    std::uint32_t stage_seed = 1;
    std::uint64_t lockstep_start_frame = 0;
    std::uint32_t lockstep_input_delay_frames = kDefaultLockstepInputDelayFrames;
    std::uint32_t lockstep_max_rollback_frames = kDefaultLockstepMaxRollbackFrames;
    std::uint64_t content_hash = 0;
    std::uint8_t multiplayer_respawn_mode = 0;
    std::array<char, kNetQuestIdBytes> quest_id{};
    std::array<char, kNetQuestStageIdBytes> quest_stage_id{};
    std::array<char, kNetNameBytes> host_name{};
};

struct JoinPendingPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    JoinPendingReason reason = JoinPendingReason::None;
    std::uint32_t pending_join_count = 0;
};

struct LeaveNoticePacket {
    std::uint32_t player_count = 0;
    std::array<PlayerId, kNetPlayersPerProcess> player_ids{};
};

struct PingPacket {
    std::uint32_t sender_peer_id = 0;
    std::uint32_t sequence = 0;
    std::uint64_t sent_time_ms = 0;
};

struct PongPacket {
    std::uint32_t sender_peer_id = 0;
    std::uint32_t sequence = 0;
    std::uint64_t echoed_sent_time_ms = 0;
};

struct InputFrameRecordEntry {
    PlayerId player_id = kInvalidPlayerId;
    std::uint64_t frame = 0;
    std::uint32_t sequence = 0;
    std::uint32_t input_flags = 0;
    std::uint32_t mouse_x = 0;
    std::uint32_t mouse_y = 0;
};

struct InputFrameRecordsPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t record_count = 0;
    std::array<InputFrameRecordEntry, kNetInputFrameRecordsPerPacket> records{};
};

struct LockstepSettingsPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t sequence = 0;
    std::uint64_t apply_frame = 0;
    std::uint32_t input_delay_frames = kDefaultLockstepInputDelayFrames;
    std::uint32_t max_rollback_frames = kDefaultLockstepMaxRollbackFrames;
};

struct LockstepHashNetPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t sync_epoch = 0;
    std::uint64_t frame = 0;
    std::uint64_t hash = 0;
    std::uint64_t component_root = 0;
    std::uint64_t component_stage = 0;
    std::uint64_t component_players = 0;
    std::uint64_t component_tools = 0;
    std::uint64_t component_ents = 0;
};

struct SnapshotResyncRequestPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint64_t mismatch_frame = 0;
};

struct SnapshotResyncChunkPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t transfer_id = 0;
    std::uint32_t chunk_index = 0;
    std::uint32_t chunk_count = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t payload_bytes = 0;
    std::uint64_t snapshot_frame = 0;
    std::array<std::uint8_t, kNetSnapshotChunkPayloadBytes> payload{};
};

struct SnapshotResyncAckPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t transfer_id = 0;
    std::uint64_t snapshot_frame = 0;
    std::uint8_t success = 0;
};

struct JoinBarrierStatusPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t barrier_id = 0;
    std::uint8_t active = 0;
    std::uint8_t phase = 0;
    PlayerId active_player_id = kInvalidPlayerId;
    std::uint32_t queued_peer_count = 0;
    std::array<PlayerId, kNetPlayersPerProcess> queued_peer_ids{};
    std::uint32_t transfer_id = 0;
    std::uint64_t snapshot_frame = 0;
    std::uint32_t chunk_count = 0;
    std::uint32_t chunks_done = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t bytes_done = 0;
};

struct JoinBarrierResumePacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t barrier_id = 0;
    std::uint64_t resume_frame = 0;
};

struct JoinBarrierTopologyPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t barrier_id = 0;
    std::uint64_t barrier_frame = 0;
    std::uint32_t player_count = 0;
    std::array<PlayerId, kNetPlayersPerProcess> player_ids{};
    std::array<float, kNetPlayersPerProcess> player_pos_x{};
    std::array<float, kNetPlayersPerProcess> player_pos_y{};
    std::uint32_t removed_player_count = 0;
    std::array<PlayerId, kNetPlayersPerProcess> removed_player_ids{};
};

struct JoinBarrierTopologyAckPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t barrier_id = 0;
    std::uint8_t success = 0;
};

struct RunRestartPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sender_peer_id = 0;
    std::uint32_t sequence = 0;
    std::uint64_t apply_frame = 0;
    std::uint32_t stage_seed = 1;
    std::array<char, kNetQuestIdBytes> quest_id{};
    std::array<char, kNetQuestStageIdBytes> quest_stage_id{};
};

struct EncodedNetPacket {
    std::array<std::uint8_t, kNetPacketMaxBytes> bytes{};
    std::size_t size = 0;
};

EncodedNetPacket EncodeJoinRequest(const JoinRequestPacket& packet);
EncodedNetPacket EncodeJoinAccept(const JoinAcceptPacket& packet);
EncodedNetPacket EncodeJoinPending(const JoinPendingPacket& packet);
EncodedNetPacket EncodePing(const PingPacket& packet);
EncodedNetPacket EncodePong(const PongPacket& packet);
EncodedNetPacket EncodeLeaveNotice(const LeaveNoticePacket& packet);
EncodedNetPacket EncodeInputFrameRecords(const InputFrameRecordsPacket& packet);
EncodedNetPacket EncodeLockstepSettings(const LockstepSettingsPacket& packet);
EncodedNetPacket EncodeLockstepHash(const LockstepHashNetPacket& packet);
EncodedNetPacket EncodeSnapshotResyncRequest(const SnapshotResyncRequestPacket& packet);
EncodedNetPacket EncodeSnapshotResyncChunk(const SnapshotResyncChunkPacket& packet);
EncodedNetPacket EncodeSnapshotResyncAck(const SnapshotResyncAckPacket& packet);
EncodedNetPacket EncodeJoinBarrierStatus(const JoinBarrierStatusPacket& packet);
EncodedNetPacket EncodeJoinBarrierResume(const JoinBarrierResumePacket& packet);
EncodedNetPacket EncodeJoinBarrierTopology(const JoinBarrierTopologyPacket& packet);
EncodedNetPacket EncodeJoinBarrierTopologyAck(const JoinBarrierTopologyAckPacket& packet);
EncodedNetPacket EncodeRunRestart(const RunRestartPacket& packet);
std::optional<JoinRequestPacket> TryDecodeJoinRequest(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinAcceptPacket> TryDecodeJoinAccept(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinPendingPacket> TryDecodeJoinPending(const std::uint8_t* bytes, std::size_t size);
std::optional<PingPacket> TryDecodePing(const std::uint8_t* bytes, std::size_t size);
std::optional<PongPacket> TryDecodePong(const std::uint8_t* bytes, std::size_t size);
std::optional<LeaveNoticePacket> TryDecodeLeaveNotice(const std::uint8_t* bytes, std::size_t size);
std::optional<InputFrameRecordsPacket> TryDecodeInputFrameRecords(const std::uint8_t* bytes, std::size_t size);
std::optional<LockstepSettingsPacket> TryDecodeLockstepSettings(const std::uint8_t* bytes, std::size_t size);
std::optional<LockstepHashNetPacket> TryDecodeLockstepHash(const std::uint8_t* bytes, std::size_t size);
std::optional<SnapshotResyncRequestPacket> TryDecodeSnapshotResyncRequest(const std::uint8_t* bytes, std::size_t size);
std::optional<SnapshotResyncChunkPacket> TryDecodeSnapshotResyncChunk(const std::uint8_t* bytes, std::size_t size);
std::optional<SnapshotResyncAckPacket> TryDecodeSnapshotResyncAck(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinBarrierStatusPacket> TryDecodeJoinBarrierStatus(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinBarrierResumePacket> TryDecodeJoinBarrierResume(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinBarrierTopologyPacket> TryDecodeJoinBarrierTopology(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinBarrierTopologyAckPacket> TryDecodeJoinBarrierTopologyAck(const std::uint8_t* bytes, std::size_t size);
std::optional<RunRestartPacket> TryDecodeRunRestart(const std::uint8_t* bytes, std::size_t size);

template <std::size_t N>
std::string ReadFixedString(const std::array<char, N>& text) {
    std::size_t count = 0;
    while (count < text.size() && text[count] != '\0') {
        ++count;
    }
    return std::string(text.data(), count);
}

template <std::size_t N>
void WriteFixedString(const std::string& text, std::array<char, N>& out) {
    out.fill('\0');
    const std::size_t count = std::min(text.size(), out.size() - 1);
    std::memcpy(out.data(), text.data(), count);
}

} // namespace splonks::network

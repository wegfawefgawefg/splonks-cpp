#pragma once

#include "effects/effect_id.hpp"
#include "ent/core_types.hpp"
#include "math_types.hpp"
#include "network/input_lockstep.hpp"
#include "network/lockstep_config.hpp"
#include "network/net_fuzzer.hpp"
#include "network/net_ids.hpp"
#include "sim/fxp.hpp"
#include "vid.hpp"
#include "tools/tool_spec.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace splonks {
struct SimSnapshot;
}

namespace splonks::network {

struct NetPeerState {
    PlayerId player_id = kInvalidPlayerId;
    std::string display_name;
    std::string endpoint_address;
    std::uint16_t endpoint_port = 0;
    float estimated_ping_ms = 0.0F;
    float jitter_ms = 0.0F;
    bool connected = false;
};

struct NetEntLink {
    NetEntId net_id = kInvalidNetEntId;
    VID local_vid{};
    std::optional<PlayerId> input_owner_player_id = std::nullopt;
};

struct NetEntIdAlias {
    NetEntId from_id = kInvalidNetEntId;
    NetEntId to_id = kInvalidNetEntId;
};

enum class NetReconnectSpawnMode : std::uint8_t {
    FreshAtEntrance,
    FreshAtHost,
    RetainedAtEntrance,
    RetainedAtLastPosition,
    RetainedAtHost,
};

constexpr std::size_t kNetRetainedToolSlotCount = 2;
constexpr std::size_t kNetRetainedEffectCount = 12;

struct NetRetainedToolSlot {
    ToolKind kind = ToolKind::ThrowPot;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    std::uint8_t active = 0;
};

struct NetRetainedEffect {
    EffectId id = EffectId::None;
    std::int32_t count = 0;
    sim::Scalar value = sim::Scalar::zero();
    std::uint32_t frames_remaining = 0;
};

struct NetRetainedAttachedEntState {
    bool valid = false;
    EntType ent_type = EntType::None;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    Vec2 size = Vec2::New(0.0F, 0.0F);
    sim::Scalar rotation = sim::Scalar::zero();
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float counter_c = 0.0F;
    float counter_d = 0.0F;
    std::uint32_t health = 0;
    std::uint32_t money = 0;
    std::uint8_t facing = 0;
    std::uint8_t condition = 0;
    std::uint8_t effect_count = 0;
    std::array<NetRetainedEffect, kNetRetainedEffectCount> effects{};
};

struct NetRetainedPlayerState {
    PlayerId player_id = kInvalidPlayerId;
    std::string display_name;
    std::string quest_id;
    std::string quest_stage_id;
    EntType ent_type = EntType::Player;
    Vec2 last_pos = Vec2::New(0.0F, 0.0F);
    std::uint32_t health = 0;
    std::uint32_t money = 0;
    std::uint64_t disconnected_frame = 0;
    std::uint8_t effect_count = 0;
    NetRetainedAttachedEntState held_item;
    NetRetainedAttachedEntState back_item;
    std::array<NetRetainedToolSlot, kNetRetainedToolSlotCount> tool_slots{};
    std::array<NetRetainedEffect, kNetRetainedEffectCount> effects{};
};

struct LockstepRollbackSnapshot {
    LockstepFrame frame = 0;
    std::shared_ptr<SimSnapshot> snapshot;
};

struct LockstepHashRecord {
    LockstepFrame frame = 0;
    std::uint64_t hash = 0;
    std::uint64_t component_root = 0;
    std::uint64_t component_stage = 0;
    std::uint64_t component_players = 0;
    std::uint64_t component_tools = 0;
    std::uint64_t component_ents = 0;
};

struct LockstepRemoteHashRecord {
    PlayerId peer_id = kInvalidPlayerId;
    LockstepFrame frame = 0;
    std::uint64_t hash = 0;
    std::uint64_t component_root = 0;
    std::uint64_t component_stage = 0;
    std::uint64_t component_players = 0;
    std::uint64_t component_tools = 0;
    std::uint64_t component_ents = 0;
};

struct LockstepEntHashDiagnostic {
    NetEntId net_ent_id = kInvalidNetEntId;
    std::uint16_t type = 0;
    std::uint64_t hash = 0;
};

struct LockstepReplayInputRecord {
    PlayerId player_id = kInvalidPlayerId;
    LockstepFrame frame = 0;
    std::uint32_t input_flags = 0;
    std::uint32_t mouse_x = 0;
    std::uint32_t mouse_y = 0;
};

struct LockstepReplayCapture {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::string quest_id;
    std::string quest_stage_id;
    std::uint32_t stage_seed = 0;
    LockstepFrame start_frame = 0;
    std::vector<std::uint8_t> initial_snapshot;
    std::vector<LockstepReplayInputRecord> inputs;
    LockstepFrame last_dump_frame = 0;
    std::uint32_t dump_count = 0;
};

struct LockstepInputArbitrationStats {
    PlayerId player_id = kInvalidPlayerId;
    std::uint64_t missing_input_count = 0;
    std::uint64_t neutral_input_count = 0;
    std::uint32_t last_missing_span = 0;
};

enum class LockstepDesyncRecoveryMode : std::uint8_t {
    None,
    PendingRollback,
    RollbackRepaired,
    SnapshotCatchup,
    FatalDesync,
};

enum class JoinBarrierPhase : std::uint8_t {
    None,
    WaitingForCatchup,
    SendingSnapshot,
    WaitingForAck,
    ReadyToResume,
    WaitingForResume,
};

struct PendingLockstepSettings {
    std::uint32_t sequence = 0;
    LockstepFrame apply_frame = 0;
    std::uint32_t input_delay_frames = kDefaultLockstepInputDelayFrames;
    std::uint32_t max_rollback_frames = kDefaultLockstepMaxRollbackFrames;
};

struct NetSessionState {
    NetRole role = NetRole::Offline;
    PlayerId local_player_id = 1;
    PlayerId host_player_id = 1;
    StageInstanceId stage_instance_id = 1;
    NetEntId next_local_ent_id = 1;
    PlayerId next_player_id = 2;
    std::string quest_id;
    std::string quest_stage_id;
    std::uint32_t stage_seed = 1;

    std::vector<NetPeerState> peers;
    std::vector<NetEntLink> ent_links;
    std::vector<NetEntIdAlias> ent_id_aliases;
    std::vector<NetRetainedPlayerState> retained_players;
    NetReconnectSpawnMode reconnect_spawn_mode = NetReconnectSpawnMode::RetainedAtLastPosition;
    std::uint64_t retained_player_lifetime_frames = 108000;
    std::uint64_t last_snapshot_expected_fingerprint = 0;
    std::uint64_t last_snapshot_actual_fingerprint = 0;
    bool last_snapshot_fingerprint_valid = false;

    NetFuzzerConfig fuzzer_config;
    NetFuzzerStats fuzzer_stats;

    bool input_lockstep_enabled = false;
    LockstepInputBuffer lockstep_input_buffer;
    LockstepFrame lockstep_next_frame_to_step = 0;
    LockstepFrame lockstep_next_local_input_frame = 0;
    std::uint32_t lockstep_input_delay_frames = kDefaultLockstepInputDelayFrames;
    std::uint32_t lockstep_next_settings_sequence = 1;
    std::optional<PendingLockstepSettings> lockstep_pending_settings = std::nullopt;
    std::optional<PendingLockstepSettings> lockstep_broadcast_settings = std::nullopt;
    LockstepFrame lockstep_broadcast_settings_until_frame = 0;
    std::uint32_t lockstep_last_applied_settings_sequence = 0;
    bool lockstep_auto_delay_enabled = false;
    std::uint32_t lockstep_auto_delay_candidate_frames = kDefaultLockstepInputDelayFrames;
    std::uint32_t lockstep_auto_delay_candidate_age_frames = 0;
    std::uint32_t lockstep_next_input_sequence = 1;
    std::uint64_t lockstep_last_confirmed_hash_frame = 0;
    std::uint64_t lockstep_last_confirmed_hash = 0;
    bool lockstep_has_confirmed_hash = false;
    std::uint32_t lockstep_hash_send_interval_frames = 30;
    std::uint64_t lockstep_hash_ignore_through_frame = 0;
    std::vector<LockstepHashRecord> lockstep_hash_history;
    std::vector<LockstepRemoteHashRecord> lockstep_remote_hash_history;
    std::vector<LockstepRemoteHashRecord> lockstep_pending_remote_hashes;
    bool lockstep_replay_capture_enabled = true;
    LockstepReplayCapture lockstep_replay_capture;
    std::string lockstep_last_desync_replay_path;
    LockstepFrame lockstep_last_recorded_hash_frame = 0;
    bool lockstep_has_recorded_hash = false;
    LockstepFrame lockstep_last_sent_hash_frame = 0;
    bool lockstep_has_sent_hash = false;
    std::uint64_t lockstep_hash_mismatch_count = 0;
    PlayerId lockstep_last_mismatch_peer_id = kInvalidPlayerId;
    LockstepFrame lockstep_last_mismatch_frame = 0;
    std::uint64_t lockstep_last_mismatch_local_hash = 0;
    std::uint64_t lockstep_last_mismatch_remote_hash = 0;
    std::uint64_t lockstep_last_mismatch_local_root = 0;
    std::uint64_t lockstep_last_mismatch_remote_root = 0;
    std::uint64_t lockstep_last_mismatch_local_stage = 0;
    std::uint64_t lockstep_last_mismatch_remote_stage = 0;
    std::uint64_t lockstep_last_mismatch_local_players = 0;
    std::uint64_t lockstep_last_mismatch_remote_players = 0;
    std::uint64_t lockstep_last_mismatch_local_tools = 0;
    std::uint64_t lockstep_last_mismatch_remote_tools = 0;
    std::uint64_t lockstep_last_mismatch_local_ents = 0;
    std::uint64_t lockstep_last_mismatch_remote_ents = 0;
    std::vector<LockstepEntHashDiagnostic> lockstep_last_mismatch_local_ent_hashes;
    LockstepDesyncRecoveryMode lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::None;
    bool lockstep_snapshot_resync_pending_request = false;
    PlayerId lockstep_snapshot_resync_target_peer_id = kInvalidPlayerId;
    std::uint32_t lockstep_snapshot_resync_next_transfer_id = 1;
    std::uint32_t lockstep_snapshot_resync_active_transfer_id = 0;
    std::uint64_t lockstep_snapshot_resync_frame = 0;
    std::uint32_t lockstep_snapshot_resync_chunk_count = 0;
    std::uint32_t lockstep_snapshot_resync_total_bytes = 0;
    std::uint32_t lockstep_snapshot_resync_next_chunk_to_send = 0;
    std::vector<std::uint8_t> lockstep_snapshot_resync_bytes;
    std::vector<std::uint8_t> lockstep_snapshot_resync_received_chunks;
    std::uint32_t lockstep_snapshot_resync_retry_ticks = 0;
    bool lockstep_snapshot_resync_waiting_for_ack = false;
    std::vector<PlayerId> lockstep_snapshot_resync_queue;
    std::uint32_t lockstep_snapshot_resync_last_acked_transfer_id = 0;
    std::uint64_t lockstep_snapshot_resync_last_acked_frame = 0;
    std::uint8_t lockstep_snapshot_resync_last_ack_success = 0;
    bool join_barrier_active = false;
    std::uint32_t join_barrier_id = 0;
    JoinBarrierPhase join_barrier_phase = JoinBarrierPhase::None;
    PlayerId join_barrier_active_peer_id = kInvalidPlayerId;
    std::vector<PlayerId> join_barrier_queue;
    std::vector<PlayerId> join_barrier_topology_ack_peers;
    std::vector<PlayerId> join_barrier_joined_player_ids;
    std::vector<PlayerId> join_barrier_removed_player_ids;
    std::uint32_t join_barrier_transfer_id = 0;
    std::uint64_t join_barrier_snapshot_frame = 0;
    std::uint32_t join_barrier_chunk_count = 0;
    std::uint32_t join_barrier_chunks_done = 0;
    std::uint32_t join_barrier_total_bytes = 0;
    std::uint32_t join_barrier_bytes_done = 0;
    bool lockstep_rollback_enabled = true;
    std::uint32_t lockstep_max_rollback_frames = kDefaultLockstepMaxRollbackFrames;
    std::optional<LockstepFrame> lockstep_rollback_requested_frame = std::nullopt;
    std::vector<LockstepRollbackSnapshot> lockstep_rollback_snapshots;
    std::uint64_t lockstep_rollback_count = 0;
    std::uint32_t lockstep_last_rollback_span = 0;
    std::uint32_t lockstep_max_rollback_span = 0;
    float lockstep_last_rollback_replay_ms = 0.0F;
    float lockstep_total_rollback_replay_ms = 0.0F;
    std::uint64_t lockstep_prediction_miss_count = 0;
    std::uint64_t lockstep_prediction_late_match_count = 0;
    std::uint32_t lockstep_last_prediction_miss_span = 0;
    std::uint64_t lockstep_input_wait_block_count = 0;
    std::uint64_t lockstep_arbitrated_missing_input_count = 0;
    std::uint64_t lockstep_arbitrated_neutral_input_count = 0;
    std::uint32_t lockstep_last_arbitrated_missing_span = 0;
    std::vector<LockstepInputArbitrationStats> lockstep_arbitration_stats_by_player;
    bool run_restart_pending = false;
    bool run_restart_transition_queued = false;
    bool run_restart_applied_locally = false;
    std::uint32_t run_restart_next_sequence = 1;
    std::uint32_t run_restart_last_sequence = 0;
    LockstepFrame run_restart_apply_frame = 0;
    std::uint32_t run_restart_stage_seed = 1;
    std::string run_restart_quest_id;
    std::string run_restart_quest_stage_id;
    std::uint64_t run_restart_packets_sent = 0;
    std::uint64_t run_restart_packets_received = 0;
    std::uint64_t run_restart_packets_accepted = 0;
    std::uint64_t run_restart_packets_ignored = 0;
    StageInstanceId run_restart_last_packet_stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t run_restart_last_packet_sender_peer_id = 0;
    std::uint32_t run_restart_last_packet_sequence = 0;
    std::uint32_t run_restart_last_packet_stage_seed = 0;
    std::string run_restart_last_ignore_reason;

    static NetSessionState NewOffline();

    NetEntId AllocateLocalEntId();

    void ClearStageEntLinks();
    void LinkEnt(NetEntId net_id, VID local_vid);
    void SetEntInputOwner(NetEntId net_id, std::optional<PlayerId> input_owner_player_id);
    void AliasEntId(NetEntId from_id, NetEntId to_id);
    NetEntId ResolveEntIdAlias(NetEntId ent_id) const;
    void UnlinkEnt(NetEntId net_id);
    std::optional<VID> FindLocalVid(NetEntId net_id) const;
    std::optional<NetEntId> FindNetEntId(VID local_vid) const;
    std::optional<PlayerId> FindEntInputOwner(NetEntId net_id) const;
    std::optional<PlayerId> FindEntInputOwner(VID local_vid) const;
};

} // namespace splonks::network

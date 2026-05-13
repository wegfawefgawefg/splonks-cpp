#pragma once

#include "effects/effect_id.hpp"
#include "ent/core_types.hpp"
#include "math_types.hpp"
#include "network/input_lockstep.hpp"
#include "network/lockstep_config.hpp"
#include "network/net_fuzzer.hpp"
#include "network/net_ids.hpp"
#include "vid.hpp"
#include "tools/tool_spec.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace splonks {
struct GameplaySnapshot;
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
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
};

struct NetRetainedAttachedEntState {
    bool valid = false;
    EntType ent_type = EntType::None;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    Vec2 size = Vec2::New(0.0F, 0.0F);
    float rotation = 0.0F;
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
    std::shared_ptr<GameplaySnapshot> snapshot;
};

struct LockstepHashRecord {
    LockstepFrame frame = 0;
    std::uint64_t hash = 0;
};

struct LockstepRemoteHashRecord {
    PlayerId peer_id = kInvalidPlayerId;
    LockstepFrame frame = 0;
    std::uint64_t hash = 0;
};

enum class LockstepDesyncRecoveryMode : std::uint8_t {
    None,
    PendingRollback,
    RollbackRepaired,
    SnapshotCatchup,
    FatalDesync,
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
    std::vector<LockstepHashRecord> lockstep_hash_history;
    std::vector<LockstepRemoteHashRecord> lockstep_remote_hash_history;
    std::vector<LockstepRemoteHashRecord> lockstep_pending_remote_hashes;
    LockstepFrame lockstep_last_recorded_hash_frame = 0;
    bool lockstep_has_recorded_hash = false;
    LockstepFrame lockstep_last_sent_hash_frame = 0;
    bool lockstep_has_sent_hash = false;
    std::uint64_t lockstep_hash_mismatch_count = 0;
    PlayerId lockstep_last_mismatch_peer_id = kInvalidPlayerId;
    LockstepFrame lockstep_last_mismatch_frame = 0;
    std::uint64_t lockstep_last_mismatch_local_hash = 0;
    std::uint64_t lockstep_last_mismatch_remote_hash = 0;
    LockstepDesyncRecoveryMode lockstep_last_desync_recovery_mode =
        LockstepDesyncRecoveryMode::None;
    bool lockstep_snapshot_resync_pending_request = false;
    PlayerId lockstep_snapshot_resync_target_peer_id = kInvalidPlayerId;
    std::uint32_t lockstep_snapshot_resync_next_transfer_id = 1;
    std::uint32_t lockstep_snapshot_resync_active_transfer_id = 0;
    std::uint64_t lockstep_snapshot_resync_frame = 0;
    std::uint32_t lockstep_snapshot_resync_chunk_count = 0;
    std::uint32_t lockstep_snapshot_resync_total_bytes = 0;
    std::vector<std::uint8_t> lockstep_snapshot_resync_bytes;
    std::vector<std::uint8_t> lockstep_snapshot_resync_received_chunks;
    std::uint32_t lockstep_snapshot_resync_retry_ticks = 0;
    bool lockstep_snapshot_resync_waiting_for_ack = false;
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

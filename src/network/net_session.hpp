#pragma once

#include "network/net_message.hpp"
#include "network/net_fuzzer.hpp"
#include "vid.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

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

struct NetEntityLink {
    NetEntityId net_id = kInvalidNetEntityId;
    VID local_vid{};
    std::optional<PlayerId> owner_player_id = std::nullopt;
};

struct NetEntityIdAlias {
    NetEntityId from_id = kInvalidNetEntityId;
    NetEntityId to_id = kInvalidNetEntityId;
};

enum class NetMessageLogPhase : std::uint8_t {
    EnqueuedOutbound,
    EnqueuedOrdered,
    Applied,
    SkippedLocalApply,
};

struct NetMessageLogEntry {
    NetMessageLogPhase phase = NetMessageLogPhase::EnqueuedOutbound;
    NetMessageId message_id = kInvalidNetMessageId;
    std::uint64_t coordinator_order = 0;
    std::uint64_t source_local_frame = 0;
    PlayerId source_player_id = kInvalidPlayerId;
    NetMessageType type = NetMessageType::None;
};

enum class NetReconnectSpawnMode : std::uint8_t {
    FreshAtEntrance,
    FreshAtHost,
    RetainedAtEntrance,
    RetainedAtLastPosition,
    RetainedAtHost,
};

struct NetRetainedAttachedEntityState {
    bool valid = false;
    EntityType entity_type = EntityType::None;
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
    std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount> effects{};
};

struct NetRetainedPlayerState {
    PlayerId player_id = kInvalidPlayerId;
    std::string display_name;
    std::string quest_id;
    std::string quest_stage_id;
    EntityType entity_type = EntityType::Player;
    Vec2 last_pos = Vec2::New(0.0F, 0.0F);
    std::uint32_t health = 0;
    std::uint32_t money = 0;
    std::uint64_t disconnected_frame = 0;
    std::uint8_t effect_count = 0;
    NetRetainedAttachedEntityState held_item;
    NetRetainedAttachedEntityState back_item;
    std::array<PlayerStatePatchedToolSlot, kPlayerStatePatchedToolSlotCount> tool_slots{};
    std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount> effects{};
};

struct NetSessionState {
    NetRole role = NetRole::Offline;
    PlayerId local_player_id = 1;
    PlayerId coordinator_player_id = 1;
    StageInstanceId stage_instance_id = 1;
    NetMessageId next_local_message_id = 1;
    NetEntityId next_local_entity_id = 1;
    PlayerId next_player_id = 2;
    std::uint64_t next_coordinator_order = 1;
    std::uint64_t next_expected_coordinator_order = 1;
    std::uint64_t highest_applied_coordinator_order = 0;
    std::string quest_id;
    std::string quest_stage_id;
    std::uint32_t stage_seed = 1;

    std::vector<NetPeerState> peers;
    std::vector<NetEntityLink> entity_links;
    std::vector<NetEntityIdAlias> entity_id_aliases;
    std::vector<NetMessage> pending_outbound_messages;
    std::vector<NetMessage> ordered_messages;
    std::vector<NetMessageId> applied_message_ids;
    std::vector<std::uint64_t> applied_coordinator_orders;
    std::vector<NetMessageLogEntry> message_log;
    std::vector<NetRetainedPlayerState> retained_players;
    std::string message_log_file_path;
    NetReconnectSpawnMode reconnect_spawn_mode = NetReconnectSpawnMode::RetainedAtLastPosition;
    std::uint64_t retained_player_lifetime_frames = 108000;
    std::uint64_t last_snapshot_expected_fingerprint = 0;
    std::uint64_t last_snapshot_actual_fingerprint = 0;
    bool last_snapshot_fingerprint_valid = false;

    NetFuzzerConfig fuzzer_config;
    NetFuzzerStats fuzzer_stats;

    static NetSessionState NewOffline();

    NetMessageHeader MakeLocalMessageHeader(std::uint64_t source_local_frame);
    NetMessageHeader MakeLocalTransientMessageHeader(std::uint64_t source_local_frame);
    NetEntityId AllocateLocalEntityId();

    void EnqueueNetMessage(NetMessage message);
    void EnqueueOrderedMessage(NetMessage message);
    void EnqueueTransientMessage(NetMessage message);
    std::size_t MarkAllOrderedMessagesApplied();
    bool MarkMessageApplied(NetMessageId message_id);
    bool HasAppliedMessage(NetMessageId message_id) const;
    void MarkCoordinatorOrderApplied(const NetMessage& message);
    void AddMessageLog(NetMessageLogPhase phase, const NetMessage& message);

    void ClearStageEntityLinks();
    void LinkEntity(NetEntityId net_id, VID local_vid);
    void SetEntityOwner(NetEntityId net_id, std::optional<PlayerId> owner_player_id);
    void AliasEntityId(NetEntityId from_id, NetEntityId to_id);
    NetEntityId ResolveEntityIdAlias(NetEntityId entity_id) const;
    void UnlinkEntity(NetEntityId net_id);
    std::optional<VID> FindLocalVid(NetEntityId net_id) const;
    std::optional<NetEntityId> FindNetEntityId(VID local_vid) const;
    std::optional<PlayerId> FindEntityOwner(NetEntityId net_id) const;
    std::optional<PlayerId> FindEntityOwner(VID local_vid) const;
    bool HasLocalAuthorityForEntity(VID local_vid) const;
};

} // namespace splonks::network

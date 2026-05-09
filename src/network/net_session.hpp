#pragma once

#include "network/net_event.hpp"
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

enum class NetEventLogPhase : std::uint8_t {
    EnqueuedOutbound,
    EnqueuedOrdered,
    Applied,
    SkippedLocalApply,
};

struct NetEventLogEntry {
    NetEventLogPhase phase = NetEventLogPhase::EnqueuedOutbound;
    NetEventId event_id = kInvalidNetEventId;
    std::uint64_t coordinator_order = 0;
    std::uint64_t source_local_frame = 0;
    PlayerId source_player_id = kInvalidPlayerId;
    NetEventType type = NetEventType::None;
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
    NetEventId next_local_event_id = 1;
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
    std::vector<NetEvent> pending_outbound_events;
    std::vector<NetEvent> ordered_events;
    std::vector<NetEventId> applied_event_ids;
    std::vector<std::uint64_t> applied_coordinator_orders;
    std::vector<NetEventLogEntry> event_log;
    std::vector<NetRetainedPlayerState> retained_players;
    std::string event_log_file_path;
    NetReconnectSpawnMode reconnect_spawn_mode = NetReconnectSpawnMode::RetainedAtLastPosition;
    std::uint64_t retained_player_lifetime_frames = 108000;
    std::uint64_t last_snapshot_expected_fingerprint = 0;
    std::uint64_t last_snapshot_actual_fingerprint = 0;
    bool last_snapshot_fingerprint_valid = false;

    NetFuzzerConfig fuzzer_config;
    NetFuzzerStats fuzzer_stats;

    static NetSessionState NewOffline();

    NetEventHeader MakeLocalEventHeader(std::uint64_t source_local_frame);
    NetEventHeader MakeLocalTransientEventHeader(std::uint64_t source_local_frame);
    NetEntityId AllocateLocalEntityId();

    void EnqueueNetEvent(NetEvent event);
    void EnqueueOrderedEvent(NetEvent event);
    void EnqueueTransientEvent(NetEvent event);
    std::size_t MarkAllOrderedEventsApplied();
    bool MarkEventApplied(NetEventId event_id);
    bool HasAppliedEvent(NetEventId event_id) const;
    void MarkCoordinatorOrderApplied(const NetEvent& event);
    void AddEventLog(NetEventLogPhase phase, const NetEvent& event);

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

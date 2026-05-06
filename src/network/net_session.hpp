#pragma once

#include "network/net_event.hpp"
#include "network/net_fuzzer.hpp"
#include "vid.hpp"

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
    std::string event_log_file_path;

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

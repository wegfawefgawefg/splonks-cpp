#include "network/net_session.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace splonks::network {

namespace {

constexpr std::size_t kMaxEventLogEntries = 256;

const char* NetRoleName(NetRole role) {
    switch (role) {
    case NetRole::Offline:
        return "offline";
    case NetRole::Coordinator:
        return "host";
    case NetRole::Peer:
        return "peer";
    }
    return "unknown";
}

const char* NetEventLogPhaseName(NetEventLogPhase phase) {
    switch (phase) {
    case NetEventLogPhase::EnqueuedLocal:
        return "local";
    case NetEventLogPhase::EnqueuedOrdered:
        return "ordered";
    case NetEventLogPhase::Applied:
        return "applied";
    case NetEventLogPhase::SkippedLocal:
        return "skip-local";
    }
    return "unknown";
}

const char* NetEventTypeName(NetEventType type) {
    switch (type) {
    case NetEventType::EntitySpawned:
        return "EntitySpawned";
    case NetEventType::EntityStatePatched:
        return "EntityStatePatched";
    case NetEventType::EntityHeld:
        return "EntityHeld";
    case NetEventType::EntityDropped:
        return "EntityDropped";
    case NetEventType::EntityThrown:
        return "EntityThrown";
    case NetEventType::EntityDamaged:
        return "EntityDamaged";
    case NetEventType::TileChanged:
        return "TileChanged";
    case NetEventType::TileBroken:
        return "TileBroken";
    case NetEventType::RopeTilePlaced:
        return "RopeTilePlaced";
    case NetEventType::MoneyChanged:
        return "MoneyChanged";
    case NetEventType::FavorChanged:
        return "FavorChanged";
    case NetEventType::StageLoaded:
        return "StageLoaded";
    default:
        return "Other";
    }
}

} // namespace

NetSessionState NetSessionState::NewOffline() {
    NetSessionState state;
    state.role = NetRole::Offline;
    state.local_player_id = 1;
    state.coordinator_player_id = 1;
    state.stage_instance_id = 1;
    state.next_local_event_id = 1;
    state.next_local_entity_id = 1;
    state.next_player_id = 2;
    state.next_coordinator_order = 1;
    state.quest_id = "classic";
    state.quest_stage_id = "classic_mines_1";
    state.stage_seed = 1;
    return state;
}

NetEventHeader NetSessionState::MakeLocalEventHeader(std::uint64_t source_local_frame) {
    NetEventHeader header;
    const std::uint64_t player_component = static_cast<std::uint64_t>(local_player_id) << 48U;
    header.event_id = player_component | next_local_event_id++;
    header.source_player_id = local_player_id;
    header.stage_instance_id = stage_instance_id;
    header.source_local_frame = source_local_frame;
    if (role == NetRole::Coordinator || role == NetRole::Offline) {
        header.coordinator_order = next_coordinator_order++;
    }
    return header;
}

NetEntityId NetSessionState::AllocateLocalEntityId() {
    const std::uint64_t player_component = static_cast<std::uint64_t>(local_player_id) << 48U;
    return player_component | next_local_entity_id++;
}

void NetSessionState::EnqueueLocalEvent(NetEvent event) {
    if (event.header.event_id == kInvalidNetEventId) {
        event.header = MakeLocalEventHeader(0);
    }
    AddEventLog(NetEventLogPhase::EnqueuedLocal, event);
    pending_local_events.push_back(event);
}

std::vector<NetEvent> NetSessionState::DrainPendingLocalEvents() {
    std::vector<NetEvent> drained;
    drained.swap(pending_local_events);
    return drained;
}

void NetSessionState::EnqueueOrderedEvent(NetEvent event) {
    if (event.header.event_id == kInvalidNetEventId) {
        event.header = MakeLocalEventHeader(0);
    }
    if (event.header.coordinator_order == 0) {
        event.header.coordinator_order = next_coordinator_order++;
    }
    AddEventLog(NetEventLogPhase::EnqueuedOrdered, event);
    ordered_events.push_back(event);
}

std::size_t NetSessionState::DrainPendingLocalEventsToOrdered() {
    std::vector<NetEvent> drained = DrainPendingLocalEvents();
    const std::size_t count = drained.size();
    for (NetEvent& event : drained) {
        EnqueueOrderedEvent(event);
    }
    return count;
}

std::size_t NetSessionState::MarkAllOrderedEventsApplied() {
    std::size_t count = 0;
    for (const NetEvent& event : ordered_events) {
        if (MarkEventApplied(event.header.event_id)) {
            ++count;
        }
    }
    return count;
}

bool NetSessionState::MarkEventApplied(NetEventId event_id) {
    if (event_id == kInvalidNetEventId || HasAppliedEvent(event_id)) {
        return false;
    }
    applied_event_ids.push_back(event_id);
    return true;
}

bool NetSessionState::HasAppliedEvent(NetEventId event_id) const {
    return std::find(applied_event_ids.begin(), applied_event_ids.end(), event_id) !=
           applied_event_ids.end();
}

void NetSessionState::AddEventLog(NetEventLogPhase phase, const NetEvent& event) {
    if (event_log.size() >= kMaxEventLogEntries) {
        event_log.erase(event_log.begin());
    }
    const NetEventLogEntry entry{
        .phase = phase,
        .event_id = event.header.event_id,
        .coordinator_order = event.header.coordinator_order,
        .source_local_frame = event.header.source_local_frame,
        .source_player_id = event.header.source_player_id,
        .type = event.type,
    };
    event_log.push_back(entry);

    const std::filesystem::path log_dir = "logs";
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    const std::filesystem::path log_path =
        log_dir / ("network_events_" + std::string(NetRoleName(role)) +
                   "_p" + std::to_string(local_player_id) + ".log");
    if (event_log_file_path != log_path.string()) {
        event_log_file_path = log_path.string();
        std::ofstream reset_file(log_path, std::ios::trunc);
        reset_file << "# splonks network event log role=" << NetRoleName(role)
                   << " local_player=" << local_player_id
                   << " stage_instance=" << stage_instance_id << '\n';
    }

    std::ofstream output(log_path, std::ios::app);
    output << "phase=" << NetEventLogPhaseName(entry.phase)
           << " event=" << entry.event_id
           << " order=" << entry.coordinator_order
           << " src_player=" << entry.source_player_id
           << " src_frame=" << entry.source_local_frame
           << " local_player=" << local_player_id
           << " stage=" << event.header.stage_instance_id
           << " type=" << NetEventTypeName(entry.type)
           << " type_id=" << static_cast<unsigned int>(entry.type)
           << '\n';
}

void NetSessionState::ClearStageEntityLinks() {
    entity_links.clear();
    applied_event_ids.clear();
    ordered_events.clear();
    pending_local_events.clear();
    event_log.clear();
}

void NetSessionState::LinkEntity(NetEntityId net_id, VID local_vid) {
    if (net_id == kInvalidNetEntityId) {
        return;
    }
    for (NetEntityLink& link : entity_links) {
        if (link.net_id == net_id) {
            link.local_vid = local_vid;
            return;
        }
    }
    entity_links.push_back(NetEntityLink{
        .net_id = net_id,
        .local_vid = local_vid,
    });
}

void NetSessionState::SetEntityOwner(
    NetEntityId net_id,
    std::optional<PlayerId> owner_player_id
) {
    if (net_id == kInvalidNetEntityId) {
        return;
    }
    if (IsPlayerNetEntityId(net_id)) {
        return;
    }
    for (NetEntityLink& link : entity_links) {
        if (link.net_id == net_id) {
            link.owner_player_id = owner_player_id;
            return;
        }
    }
    entity_links.push_back(NetEntityLink{
        .net_id = net_id,
        .local_vid = VID{},
        .owner_player_id = owner_player_id,
    });
}

void NetSessionState::UnlinkEntity(NetEntityId net_id) {
    entity_links.erase(
        std::remove_if(
            entity_links.begin(),
            entity_links.end(),
            [net_id](const NetEntityLink& link) { return link.net_id == net_id; }
        ),
        entity_links.end()
    );
}

std::optional<VID> NetSessionState::FindLocalVid(NetEntityId net_id) const {
    for (const NetEntityLink& link : entity_links) {
        if (link.net_id == net_id) {
            return link.local_vid;
        }
    }
    return std::nullopt;
}

std::optional<NetEntityId> NetSessionState::FindNetEntityId(VID local_vid) const {
    for (const NetEntityLink& link : entity_links) {
        if (link.local_vid == local_vid) {
            return link.net_id;
        }
    }
    return std::nullopt;
}

std::optional<PlayerId> NetSessionState::FindEntityOwner(NetEntityId net_id) const {
    if (IsPlayerNetEntityId(net_id)) {
        return GetPlayerIdFromNetEntityId(net_id);
    }
    for (const NetEntityLink& link : entity_links) {
        if (link.net_id == net_id) {
            return link.owner_player_id;
        }
    }
    return GetSpawnedNetEntityOwnerPlayerId(net_id);
}

std::optional<PlayerId> NetSessionState::FindEntityOwner(VID local_vid) const {
    for (const NetEntityLink& link : entity_links) {
        if (link.local_vid == local_vid) {
            if (IsPlayerNetEntityId(link.net_id)) {
                return GetPlayerIdFromNetEntityId(link.net_id);
            }
            if (link.owner_player_id.has_value()) {
                return link.owner_player_id;
            }
            return GetSpawnedNetEntityOwnerPlayerId(link.net_id);
        }
    }
    return std::nullopt;
}

bool NetSessionState::HasLocalAuthorityForEntity(VID local_vid) const {
    if (role == NetRole::Offline) {
        return true;
    }
    if (const std::optional<PlayerId> owner_player_id = FindEntityOwner(local_vid)) {
        return *owner_player_id == local_player_id;
    }
    return role == NetRole::Coordinator;
}

} // namespace splonks::network

#include "network/net_session.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace splonks::network {

namespace {

constexpr std::size_t kMaxMessageLogEntries = 256;
constexpr std::size_t kMaxAppliedCoordinatorOrders = 1024;

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

const char* NetMessageLogPhaseName(NetMessageLogPhase phase) {
    switch (phase) {
    case NetMessageLogPhase::EnqueuedOutbound:
        return "outbound";
    case NetMessageLogPhase::EnqueuedOrdered:
        return "ordered";
    case NetMessageLogPhase::Applied:
        return "applied";
    case NetMessageLogPhase::SkippedLocalApply:
        return "skip-local";
    }
    return "unknown";
}

const char* NetMessageTypeName(NetMessageType type) {
    switch (type) {
    case NetMessageType::EntitySpawned:
        return "EntitySpawned";
    case NetMessageType::EntityDeactivated:
        return "EntityDeactivated";
    case NetMessageType::EntityStatePatched:
        return "EntityStatePatched";
    case NetMessageType::EntityHeld:
        return "EntityHeld";
    case NetMessageType::EntityDropped:
        return "EntityDropped";
    case NetMessageType::EntityThrown:
        return "EntityThrown";
    case NetMessageType::EntityDamaged:
        return "EntityDamaged";
    case NetMessageType::TileChanged:
        return "TileChanged";
    case NetMessageType::FluidCellPatched:
        return "FluidCellPatched";
    case NetMessageType::StageLightAdded:
        return "StageLightAdded";
    case NetMessageType::StageLightRemoved:
        return "StageLightRemoved";
    case NetMessageType::TileBroken:
        return "TileBroken";
    case NetMessageType::PlayerStatePatched:
        return "PlayerStatePatched";
    case NetMessageType::RunStatePatched:
        return "RunStatePatched";
    case NetMessageType::PresentationCommand:
        return "PresentationCommand";
    case NetMessageType::StageLoaded:
        return "StageLoaded";
    default:
        return "Other";
    }
}

bool IsTransientStatePatch(const NetMessage& message) {
    if (message.type == NetMessageType::FluidCellPatched) {
        return true;
    }
    if (message.type != NetMessageType::EntityStatePatched) {
        return false;
    }
    const auto* const payload = std::get_if<EntityStatePatchedMessage>(&message.payload);
    return payload == nullptr || payload->source_entity_id == kInvalidNetEntityId;
}

} // namespace

NetSessionState NetSessionState::NewOffline() {
    NetSessionState state;
    state.role = NetRole::Offline;
    state.local_player_id = 1;
    state.coordinator_player_id = 1;
    state.stage_instance_id = 1;
    state.next_local_message_id = 1;
    state.next_local_entity_id = 1;
    state.next_player_id = 2;
    state.next_coordinator_order = 1;
    state.next_expected_coordinator_order = 1;
    state.highest_applied_coordinator_order = 0;
    state.quest_id = "classic";
    state.quest_stage_id = "classic_mines_1";
    state.stage_seed = 1;
    return state;
}

NetMessageHeader NetSessionState::MakeLocalMessageHeader(std::uint64_t source_local_frame) {
    NetMessageHeader header = MakeLocalTransientMessageHeader(source_local_frame);
    if (role == NetRole::Coordinator || role == NetRole::Offline) {
        header.coordinator_order = next_coordinator_order++;
    }
    return header;
}

NetMessageHeader NetSessionState::MakeLocalTransientMessageHeader(std::uint64_t source_local_frame) {
    NetMessageHeader header;
    const std::uint64_t player_component = static_cast<std::uint64_t>(local_player_id) << 48U;
    header.message_id = player_component | next_local_message_id++;
    header.source_player_id = local_player_id;
    header.stage_instance_id = stage_instance_id;
    header.source_local_frame = source_local_frame;
    return header;
}

NetEntityId NetSessionState::AllocateLocalEntityId() {
    const std::uint64_t player_component = static_cast<std::uint64_t>(local_player_id) << 48U;
    return player_component | next_local_entity_id++;
}

void NetSessionState::EnqueueNetMessage(NetMessage message) {
    if (message.header.message_id == kInvalidNetMessageId) {
        message.header = MakeLocalMessageHeader(0);
    }
    if (role == NetRole::Coordinator || role == NetRole::Offline) {
        EnqueueOrderedMessage(message);
        return;
    }
    AddMessageLog(NetMessageLogPhase::EnqueuedOutbound, message);
    pending_outbound_messages.push_back(message);
}

void NetSessionState::EnqueueOrderedMessage(NetMessage message) {
    if (message.header.message_id == kInvalidNetMessageId) {
        message.header = MakeLocalMessageHeader(0);
    }
    if (IsTransientStatePatch(message)) {
        EnqueueTransientMessage(message);
        return;
    }
    if (message.header.coordinator_order == 0) {
        message.header.coordinator_order = next_coordinator_order++;
    }
    AddMessageLog(NetMessageLogPhase::EnqueuedOrdered, message);
    ordered_messages.push_back(message);
}

void NetSessionState::EnqueueTransientMessage(NetMessage message) {
    if (message.header.message_id == kInvalidNetMessageId) {
        message.header = MakeLocalTransientMessageHeader(0);
    }
    message.header.coordinator_order = 0;
    AddMessageLog(NetMessageLogPhase::EnqueuedOrdered, message);
    ordered_messages.push_back(message);
}

std::size_t NetSessionState::MarkAllOrderedMessagesApplied() {
    std::size_t count = 0;
    for (const NetMessage& message : ordered_messages) {
        if (MarkMessageApplied(message.header.message_id)) {
            ++count;
        }
    }
    return count;
}

bool NetSessionState::MarkMessageApplied(NetMessageId message_id) {
    if (message_id == kInvalidNetMessageId || HasAppliedMessage(message_id)) {
        return false;
    }
    applied_message_ids.push_back(message_id);
    return true;
}

bool NetSessionState::HasAppliedMessage(NetMessageId message_id) const {
    return std::find(applied_message_ids.begin(), applied_message_ids.end(), message_id) !=
           applied_message_ids.end();
}

void NetSessionState::MarkCoordinatorOrderApplied(const NetMessage& message) {
    const std::uint64_t order = message.header.coordinator_order;
    if (order == 0) {
        return;
    }

    highest_applied_coordinator_order = std::max(highest_applied_coordinator_order, order);
    if (std::find(applied_coordinator_orders.begin(), applied_coordinator_orders.end(), order) ==
        applied_coordinator_orders.end()) {
        applied_coordinator_orders.push_back(order);
    }

    if (role == NetRole::Peer) {
        while (std::find(
                   applied_coordinator_orders.begin(),
                   applied_coordinator_orders.end(),
                   next_expected_coordinator_order
               ) != applied_coordinator_orders.end()) {
            ++next_expected_coordinator_order;
        }
    }

    if (applied_coordinator_orders.size() > kMaxAppliedCoordinatorOrders) {
        const std::uint64_t prune_before =
            next_expected_coordinator_order > 256 ? next_expected_coordinator_order - 256 : 0;
        applied_coordinator_orders.erase(
            std::remove_if(
                applied_coordinator_orders.begin(),
                applied_coordinator_orders.end(),
                [prune_before](std::uint64_t applied_order) {
                    return applied_order < prune_before;
                }
            ),
            applied_coordinator_orders.end()
        );
    }
}

void NetSessionState::AddMessageLog(NetMessageLogPhase phase, const NetMessage& message) {
    if (message_log.size() >= kMaxMessageLogEntries) {
        message_log.erase(message_log.begin());
    }
    const NetMessageLogEntry entry{
        .phase = phase,
        .message_id = message.header.message_id,
        .coordinator_order = message.header.coordinator_order,
        .source_local_frame = message.header.source_local_frame,
        .source_player_id = message.header.source_player_id,
        .type = message.type,
    };
    message_log.push_back(entry);

    const std::filesystem::path log_dir = "logs";
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    const std::filesystem::path log_path =
        log_dir / ("network_messages_" + std::string(NetRoleName(role)) +
                   "_p" + std::to_string(local_player_id) + ".log");
    if (message_log_file_path != log_path.string()) {
        message_log_file_path = log_path.string();
        std::ofstream reset_file(log_path, std::ios::trunc);
        reset_file << "# splonks network message log role=" << NetRoleName(role)
                   << " local_player=" << local_player_id
                   << " stage_instance=" << stage_instance_id << '\n';
    }

    std::ofstream output(log_path, std::ios::app);
    output << "phase=" << NetMessageLogPhaseName(entry.phase)
           << " message=" << entry.message_id
           << " order=" << entry.coordinator_order
           << " src_player=" << entry.source_player_id
           << " src_frame=" << entry.source_local_frame
           << " local_player=" << local_player_id
           << " stage=" << message.header.stage_instance_id
           << " type=" << NetMessageTypeName(entry.type)
           << " type_id=" << static_cast<unsigned int>(entry.type);
    if (const auto* const state_patch = std::get_if<EntityStatePatchedMessage>(&message.payload)) {
        output << " entity=" << state_patch->entity_id
               << " source_entity=" << state_patch->source_entity_id;
    }
    output << '\n';
}

void NetSessionState::ClearStageEntityLinks() {
    entity_links.clear();
    entity_id_aliases.clear();
    applied_message_ids.clear();
    applied_coordinator_orders.clear();
    ordered_messages.clear();
    pending_outbound_messages.clear();
    message_log.clear();
    next_coordinator_order = 1;
    next_expected_coordinator_order = 1;
    highest_applied_coordinator_order = 0;
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

void NetSessionState::AliasEntityId(NetEntityId from_id, NetEntityId to_id) {
    if (from_id == kInvalidNetEntityId ||
        to_id == kInvalidNetEntityId ||
        from_id == to_id) {
        return;
    }
    for (NetEntityIdAlias& alias : entity_id_aliases) {
        if (alias.from_id == from_id) {
            alias.to_id = to_id;
            return;
        }
    }
    entity_id_aliases.push_back(NetEntityIdAlias{
        .from_id = from_id,
        .to_id = to_id,
    });
}

NetEntityId NetSessionState::ResolveEntityIdAlias(NetEntityId entity_id) const {
    constexpr int kMaxAliasDepth = 8;
    NetEntityId resolved = entity_id;
    for (int depth = 0; depth < kMaxAliasDepth; ++depth) {
        bool changed = false;
        for (const NetEntityIdAlias& alias : entity_id_aliases) {
            if (alias.from_id == resolved) {
                resolved = alias.to_id;
                changed = true;
                break;
            }
        }
        if (!changed) {
            break;
        }
    }
    return resolved;
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
            return link.owner_player_id;
        }
    }
    return std::nullopt;
}

bool NetSessionState::HasLocalAuthorityForEntity(VID local_vid) const {
    if (role == NetRole::Offline) {
        return true;
    }
    if (role == NetRole::Coordinator) {
        return true;
    }
    if (const std::optional<PlayerId> owner_player_id = FindEntityOwner(local_vid)) {
        return *owner_player_id == local_player_id;
    }
    return false;
}

} // namespace splonks::network

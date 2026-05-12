#include "network/net_session.hpp"

#include <algorithm>

namespace splonks::network {

NetSessionState NetSessionState::NewOffline() {
    NetSessionState state;
    state.role = NetRole::Offline;
    state.local_player_id = 1;
    state.coordinator_player_id = 1;
    state.stage_instance_id = 1;
    state.next_local_entity_id = 1;
    state.next_player_id = 2;
    state.quest_id = "classic";
    state.quest_stage_id = "classic_mines_1";
    state.stage_seed = 1;
    return state;
}

NetEntityId NetSessionState::AllocateLocalEntityId() {
    const std::uint64_t player_component = static_cast<std::uint64_t>(local_player_id) << 48U;
    return player_component | next_local_entity_id++;
}

void NetSessionState::ClearStageEntityLinks() {
    entity_links.clear();
    entity_id_aliases.clear();
    next_local_entity_id = 1;
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
    if (net_id == kInvalidNetEntityId || IsPlayerNetEntityId(net_id)) {
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
    if (role == NetRole::Offline || role == NetRole::Coordinator) {
        return true;
    }
    if (const std::optional<PlayerId> owner_player_id = FindEntityOwner(local_vid)) {
        return *owner_player_id == local_player_id;
    }
    return false;
}

} // namespace splonks::network

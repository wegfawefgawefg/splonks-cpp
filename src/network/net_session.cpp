#include "network/net_session.hpp"

#include <algorithm>

namespace splonks::network {

NetSessionState NetSessionState::NewOffline() {
    NetSessionState state;
    state.role = NetRole::Offline;
    state.local_player_id = 1;
    state.host_player_id = 1;
    state.stage_instance_id = 1;
    state.next_local_ent_id = 1;
    state.next_player_id = 2;
    state.quest_id = "classic";
    state.quest_stage_id = "classic_mines_1";
    state.stage_seed = 1;
    return state;
}

NetEntId NetSessionState::AllocateLocalEntId() {
    const std::uint64_t player_component = static_cast<std::uint64_t>(local_player_id) << 48U;
    return player_component | next_local_ent_id++;
}

void NetSessionState::ClearStageEntLinks() {
    ent_links.clear();
    ent_id_aliases.clear();
    next_local_ent_id = 1;
}

void NetSessionState::LinkEnt(NetEntId net_id, VID local_vid) {
    if (net_id == kInvalidNetEntId) {
        return;
    }
    for (NetEntLink& link : ent_links) {
        if (link.net_id == net_id) {
            link.local_vid = local_vid;
            return;
        }
    }
    ent_links.push_back(NetEntLink{
        .net_id = net_id,
        .local_vid = local_vid,
    });
}

void NetSessionState::SetEntInputOwner(
    NetEntId net_id,
    std::optional<PlayerId> input_owner_player_id
) {
    if (net_id == kInvalidNetEntId || IsPlayerNetEntId(net_id)) {
        return;
    }
    for (NetEntLink& link : ent_links) {
        if (link.net_id == net_id) {
            link.input_owner_player_id = input_owner_player_id;
            return;
        }
    }
    ent_links.push_back(NetEntLink{
        .net_id = net_id,
        .local_vid = VID{},
        .input_owner_player_id = input_owner_player_id,
    });
}

void NetSessionState::AliasEntId(NetEntId from_id, NetEntId to_id) {
    if (from_id == kInvalidNetEntId ||
        to_id == kInvalidNetEntId ||
        from_id == to_id) {
        return;
    }
    for (NetEntIdAlias& alias : ent_id_aliases) {
        if (alias.from_id == from_id) {
            alias.to_id = to_id;
            return;
        }
    }
    ent_id_aliases.push_back(NetEntIdAlias{
        .from_id = from_id,
        .to_id = to_id,
    });
}

NetEntId NetSessionState::ResolveEntIdAlias(NetEntId ent_id) const {
    constexpr int kMaxAliasDepth = 8;
    NetEntId resolved = ent_id;
    for (int depth = 0; depth < kMaxAliasDepth; ++depth) {
        bool changed = false;
        for (const NetEntIdAlias& alias : ent_id_aliases) {
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

void NetSessionState::UnlinkEnt(NetEntId net_id) {
    ent_links.erase(
        std::remove_if(
            ent_links.begin(),
            ent_links.end(),
            [net_id](const NetEntLink& link) { return link.net_id == net_id; }
        ),
        ent_links.end()
    );
}

std::optional<VID> NetSessionState::FindLocalVid(NetEntId net_id) const {
    for (const NetEntLink& link : ent_links) {
        if (link.net_id == net_id) {
            return link.local_vid;
        }
    }
    return std::nullopt;
}

std::optional<NetEntId> NetSessionState::FindNetEntId(VID local_vid) const {
    for (const NetEntLink& link : ent_links) {
        if (link.local_vid == local_vid) {
            return link.net_id;
        }
    }
    return std::nullopt;
}

std::optional<PlayerId> NetSessionState::FindEntInputOwner(NetEntId net_id) const {
    if (IsPlayerNetEntId(net_id)) {
        return GetPlayerIdFromNetEntId(net_id);
    }
    for (const NetEntLink& link : ent_links) {
        if (link.net_id == net_id) {
            return link.input_owner_player_id;
        }
    }
    return GetSpawnedNetEntInputOwnerPlayerId(net_id);
}

std::optional<PlayerId> NetSessionState::FindEntInputOwner(VID local_vid) const {
    for (const NetEntLink& link : ent_links) {
        if (link.local_vid == local_vid) {
            if (IsPlayerNetEntId(link.net_id)) {
                return GetPlayerIdFromNetEntId(link.net_id);
            }
            return link.input_owner_player_id;
        }
    }
    return std::nullopt;
}

} // namespace splonks::network

#include "network/net_ent_links.hpp"

#include "ent.hpp"
#include "network/net_session.hpp"
#include "state.hpp"

namespace splonks::network {

namespace {

NetEntId MakeStageEntId(std::uint32_t stage_ent_index) {
    return static_cast<NetEntId>(stage_ent_index) + 1U;
}

bool IsStageLinkedEnt(const State& state, const Ent& ent) {
    return ent.active &&
           ent.stage_spawn_index.has_value() &&
           !state.players.FindPlayerIdForEnt(ent.vid).has_value();
}

void RegisterPlayerEntLinks(State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.ent_vid.has_value()) {
            continue;
        }

        const Ent* const ent = state.ents.GetEnt(*slot.ent_vid);
        if (ent == nullptr || !ent->active) {
            continue;
        }

        state.net_session.LinkEnt(MakePlayerNetEntId(slot.player_id), ent->vid);
    }
}

} // namespace

void RegisterStageEntLinks(State& state) {
    if (state.net_session.role == NetRole::Offline) {
        return;
    }

    RegisterPlayerEntLinks(state);

    for (const Ent& ent : state.ents.ents) {
        if (!IsStageLinkedEnt(state, ent)) {
            continue;
        }
        if (state.net_session.FindNetEntId(ent.vid).has_value()) {
            continue;
        }
        state.net_session.LinkEnt(MakeStageEntId(*ent.stage_spawn_index), ent.vid);
    }
}

NetEntId GetOrAssignReplicatedEntId(State& state, VID ent_vid) {
    if (const std::optional<NetEntId> linked = state.net_session.FindNetEntId(ent_vid)) {
        return *linked;
    }

    if (const std::optional<PlayerId> player_id = state.players.FindPlayerIdForEnt(ent_vid)) {
        const NetEntId player_ent_id = MakePlayerNetEntId(*player_id);
        state.net_session.LinkEnt(player_ent_id, ent_vid);
        return player_ent_id;
    }

    const NetEntId runtime_id = state.net_session.AllocateLocalEntId();
    state.net_session.LinkEnt(runtime_id, ent_vid);
    state.net_session.SetEntInputOwner(runtime_id, std::nullopt);
    return runtime_id;
}

} // namespace splonks::network

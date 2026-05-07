#include "network/net_entity_links.hpp"

#include "entity.hpp"
#include "network/net_session.hpp"
#include "state.hpp"

namespace splonks::network {

namespace {

NetEntityId MakeStageEntityId(std::size_t stage_entity_index) {
    return static_cast<NetEntityId>(stage_entity_index) + 1U;
}

bool IsStageLinkedEntity(const State& state, const Entity& entity) {
    return entity.active &&
           entity.stage_spawn_index.has_value() &&
           !state.players.FindPlayerIdForEntity(entity.vid).has_value();
}

void RegisterPlayerEntityLinks(State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.entity_vid.has_value()) {
            continue;
        }

        const Entity* const entity = state.entity_manager.GetEntity(*slot.entity_vid);
        if (entity == nullptr || !entity->active) {
            continue;
        }

        state.net_session.LinkEntity(MakePlayerNetEntityId(slot.player_id), entity->vid);
    }
}

} // namespace

void RegisterStageEntityLinks(State& state) {
    if (state.net_session.role == NetRole::Offline) {
        return;
    }

    RegisterPlayerEntityLinks(state);

    for (const Entity& entity : state.entity_manager.entities) {
        if (!IsStageLinkedEntity(state, entity)) {
            continue;
        }
        if (state.net_session.FindNetEntityId(entity.vid).has_value()) {
            continue;
        }
        state.net_session.LinkEntity(MakeStageEntityId(*entity.stage_spawn_index), entity.vid);
    }
}

NetEntityId GetOrAssignReplicatedEntityId(State& state, VID entity_vid) {
    if (const std::optional<NetEntityId> linked = state.net_session.FindNetEntityId(entity_vid)) {
        return *linked;
    }

    if (const std::optional<PlayerId> player_id = state.players.FindPlayerIdForEntity(entity_vid)) {
        const NetEntityId player_entity_id = MakePlayerNetEntityId(*player_id);
        state.net_session.LinkEntity(player_entity_id, entity_vid);
        return player_entity_id;
    }

    const NetEntityId runtime_id = state.net_session.AllocateLocalEntityId();
    state.net_session.LinkEntity(runtime_id, entity_vid);
    state.net_session.SetEntityOwner(runtime_id, std::nullopt);
    return runtime_id;
}

} // namespace splonks::network

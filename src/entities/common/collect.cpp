#include "entities/common/common.hpp"

#include "gameplay_events.hpp"
#include "network/net_ids.hpp"

namespace splonks::entities::common {

bool CanCollectPickupFromContact(
    std::size_t pickup_idx,
    std::size_t collector_idx,
    const State& state
) {
    if (pickup_idx >= state.entity_manager.entities.size() ||
        collector_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& pickup = state.entity_manager.entities[pickup_idx];
    const Entity& collector = state.entity_manager.entities[collector_idx];
    if (!pickup.active || !collector.active || !collector.can_collect_pickups) {
        return false;
    }

    return !pickup.buyable.active;
}

bool TryRequestCollectPickupFromContact(
    std::size_t pickup_idx,
    std::size_t collector_idx,
    State& state
) {
    if (state.net_session.role != network::NetRole::Peer ||
        !CanCollectPickupFromContact(pickup_idx, collector_idx, state)) {
        return false;
    }

    Entity& pickup = state.entity_manager.entities[pickup_idx];
    Entity& collector = state.entity_manager.entities[collector_idx];
    const PlayerSlot* const collector_slot = state.players.FindByEntityVid(collector.vid);
    if (collector_slot == nullptr ||
        collector_slot->connection_kind != PlayerConnectionKind::Local) {
        return false;
    }

    EmitGameplayActionRequested(
        state,
        GameplayActionRequested{
            .kind = GameplayActionKind::CollectEntity,
            .source_vid = collector.vid,
            .target_vid = pickup.vid,
        }
    );
    return true;
}

void DeactivateCollectedPickup(std::size_t pickup_idx, State& state, const Graphics& graphics) {
    if (pickup_idx >= state.entity_manager.entities.size()) {
        return;
    }

    EmitEntityDeactivatedGameplayEvent(state, state.entity_manager.entities[pickup_idx]);
    state.entity_manager.SetInactive(pickup_idx);
    state.UpdateSidForEntity(pickup_idx, graphics);
}

} // namespace splonks::entities::common

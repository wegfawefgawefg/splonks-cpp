#include "entities/common/common.hpp"

#include "gameplay_events.hpp"

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

void DeactivateCollectedPickup(std::size_t pickup_idx, State& state, const Graphics& graphics) {
    if (pickup_idx >= state.entity_manager.entities.size()) {
        return;
    }

    EmitEntityDeactivatedGameplayEvent(state, state.entity_manager.entities[pickup_idx]);
    state.entity_manager.SetInactive(pickup_idx);
    state.UpdateSidForEntity(pickup_idx, graphics);
}

} // namespace splonks::entities::common

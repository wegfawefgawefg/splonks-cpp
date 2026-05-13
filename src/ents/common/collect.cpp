#include "ents/common/common.hpp"

#include "world_ops.hpp"

namespace splonks::ents::common {

bool CanCollectPickupFromContact(
    std::size_t pickup_idx,
    std::size_t collector_idx,
    const State& state
) {
    if (pickup_idx >= state.ents.ents.size() ||
        collector_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& pickup = state.ents.ents[pickup_idx];
    const Ent& collector = state.ents.ents[collector_idx];
    if (!pickup.active || !collector.active || !collector.can_collect_pickups) {
        return false;
    }

    return !pickup.buyable.active;
}

void DeactivateCollectedPickup(std::size_t pickup_idx, State& state, const Graphics& graphics) {
    if (pickup_idx >= state.ents.ents.size()) {
        return;
    }

    (void)world_ops::DeactivateEnt(state, state.ents.ents[pickup_idx].vid);
    state.UpdateSidForEnt(pickup_idx, graphics);
}

} // namespace splonks::ents::common

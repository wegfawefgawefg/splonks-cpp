#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::common {
struct ContactContext;
}

namespace splonks::entities::breakaway_container {

void SetEntityBreakawayContainer(Entity& entity, EntityType type_);
void StepEntityLogicAsBreakawayContainer(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsBreakawayContainer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
Vec2 GetBreakawayContainerSize(EntityType type_);
bool TryApplyBreakawayContainerImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
);

} // namespace splonks::entities::breakaway_container

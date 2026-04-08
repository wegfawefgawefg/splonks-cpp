#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

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

} // namespace splonks::entities::breakaway_container

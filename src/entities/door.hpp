#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::door {

extern const EntityArchetype kDoorArchetype;

void StepEntityLogicAsDoor(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntityPhysicsAsDoor(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::door

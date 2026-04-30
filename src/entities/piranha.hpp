#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::piranha {

extern const EntityArchetype kPiranhaArchetype;

void StepEntityLogicAsPiranha(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntityPhysicsAsPiranha(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::piranha

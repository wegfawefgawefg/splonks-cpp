#pragma once

#include "entity_archetype.hpp"
#include "entities/common.hpp"

namespace splonks::entities::bow {

extern const EntityArchetype kBowArchetype;

void StepEntityLogicAsBow(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsBow(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::bow

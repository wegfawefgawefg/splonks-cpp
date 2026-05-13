#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::trap_block {

extern const EntityArchetype kTrapBlockArchetype;

void MakeTrapBlockOneShot(Entity& block);

void StepEntityLogicAsTrapBlock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntityPhysicsAsTrapBlock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::trap_block

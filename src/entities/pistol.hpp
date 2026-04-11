#pragma once

#include "entity_archetype.hpp"
#include "entities/common.hpp"

namespace splonks::entities::pistol {

extern const EntityArchetype kPistolArchetype;

void StepEntityLogicAsPistol(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsPistol(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::pistol

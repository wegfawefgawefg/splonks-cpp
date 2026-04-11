#pragma once

#include "entity_archetype.hpp"
#include "entities/common.hpp"

namespace splonks::entities::machete {

extern const EntityArchetype kMacheteArchetype;

void StepEntityLogicAsMachete(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsMachete(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::machete

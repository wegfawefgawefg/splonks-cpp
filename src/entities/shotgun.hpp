#pragma once

#include "entity_archetype.hpp"
#include "entities/common.hpp"

namespace splonks::entities::shotgun {

extern const EntityArchetype kShotgunArchetype;

void StepEntityLogicAsShotgun(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsShotgun(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::shotgun

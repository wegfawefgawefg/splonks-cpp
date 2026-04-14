#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::damsel {

void StepEntityLogicAsDamsel(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

extern const EntityArchetype kDamselArchetype;

} // namespace splonks::entities::damsel

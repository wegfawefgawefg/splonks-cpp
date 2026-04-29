#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::mantrap {

extern const EntityArchetype kMantrapArchetype;

void StepEntityLogicAsMantrap(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::mantrap

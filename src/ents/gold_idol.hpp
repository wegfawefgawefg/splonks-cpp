#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::gold_idol {

void StepEntityLogicAsGoldIdol(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

extern const EntityArchetype kGoldIdolArchetype;

} // namespace splonks::entities::gold_idol

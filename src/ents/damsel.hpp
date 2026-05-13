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

bool BuyDamsel(
    std::size_t entity_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);

extern const EntityArchetype kDamselArchetype;

} // namespace splonks::entities::damsel

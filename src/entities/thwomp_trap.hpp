#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::thwomp_trap {

extern const EntityArchetype kThwompTrapArchetype;

void StepEntityLogicAsThwompTrap(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntityPhysicsAsThwompTrap(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::thwomp_trap

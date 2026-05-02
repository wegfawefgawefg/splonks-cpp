#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::entrance {

extern const EntityArchetype kEntranceArchetype;

void StepEntityLogicAsEntrance(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::entrance

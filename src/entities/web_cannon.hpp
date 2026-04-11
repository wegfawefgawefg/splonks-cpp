#pragma once

#include "entity_archetype.hpp"
#include "entities/common.hpp"

namespace splonks::entities::web_cannon {

extern const EntityArchetype kWebCannonArchetype;

void StepEntityLogicAsWebCannon(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsWebCannon(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::web_cannon

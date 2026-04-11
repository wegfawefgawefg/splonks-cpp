#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::damsel {

extern const EntityArchetype kDamselArchetype;

void StepEntityLogicAsDamsel(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsDamsel(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::damsel

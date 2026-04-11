#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::dice {

extern const EntityArchetype kDiceArchetype;

void StepEntityLogicAsDice(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsDice(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::dice

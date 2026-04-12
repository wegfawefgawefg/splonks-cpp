#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::bomb {

extern const EntityArchetype kBombArchetype;

void OnDeathAsBomb(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityLogicAsBomb(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsBomb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::bomb

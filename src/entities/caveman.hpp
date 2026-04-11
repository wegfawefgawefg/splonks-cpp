#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::caveman {

extern const EntityArchetype kCavemanArchetype;

void StepEntityLogicAsCaveman(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsCaveman(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::caveman

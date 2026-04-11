#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::snake {

extern const EntityArchetype kSnakeArchetype;

void StepEntityLogicAsSnake(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsSnake(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::snake

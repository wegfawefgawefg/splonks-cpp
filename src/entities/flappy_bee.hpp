#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::flappy_bee {

extern const EntityArchetype kFlappyBeeArchetype;

void OnDeathAsFlappyBee(std::size_t entity_idx, State& state, Audio& audio);
void ControlEntityAsFlappyBee(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntityLogicAsFlappyBee(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntityPhysicsAsFlappyBee(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::flappy_bee

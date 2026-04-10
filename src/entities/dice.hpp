#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::dice {

void SetEntityDice(Entity& entity);
void StepEntityLogicAsDice(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsDice(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::dice

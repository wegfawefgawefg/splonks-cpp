#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::rock {

void SetEntityRock(Entity& entity);
void StepEntityLogicAsRock(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsRock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::rock

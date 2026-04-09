#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::bomb {

void SetEntityBomb(Entity& entity);
void StepEntityLogicAsBomb(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntityPhysicsAsBomb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::bomb

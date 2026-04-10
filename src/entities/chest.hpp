#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::chest {

void SetEntityChest(Entity& entity);
void StepEntityLogicAsChest(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsChest(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::chest

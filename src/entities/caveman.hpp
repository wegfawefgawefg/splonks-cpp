#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::caveman {

void SetEntityCaveman(Entity& entity);
void StepEntityLogicAsCaveman(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsCaveman(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::caveman

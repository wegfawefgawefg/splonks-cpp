#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::mattock {

void SetEntityMattock(Entity& entity);
void StepEntityLogicAsMattock(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsMattock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::mattock

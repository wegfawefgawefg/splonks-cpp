#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::scarab {

void SetEntityScarab(Entity& entity);
void StepEntityLogicAsScarab(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsScarab(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::scarab

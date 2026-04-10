#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::damsel {

void SetEntityDamsel(Entity& entity);
void StepEntityLogicAsDamsel(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsDamsel(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::damsel

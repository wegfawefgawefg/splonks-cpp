#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::gold_idol {

void SetEntityGoldIdol(Entity& entity);
void StepEntityLogicAsGoldIdol(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsGoldIdol(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::gold_idol

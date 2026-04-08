#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::block {

constexpr float kBlockPushAcc = 0.4F;

void SetEntityBlock(Entity& entity);
void StepEntityLogicAsBlock(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsBlock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::block

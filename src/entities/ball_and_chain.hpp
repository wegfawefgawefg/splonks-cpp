#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::ball_and_chain {

extern const EntityArchetype kBallAndChainBallArchetype;

void StepEntityLogicAsBallAndChainBall(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::ball_and_chain

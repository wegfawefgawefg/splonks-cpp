#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::ball_and_chain {

extern const EntSpec kBallAndChainBallSpec;

void StepEntLogicAsBallAndChainBall(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::ball_and_chain

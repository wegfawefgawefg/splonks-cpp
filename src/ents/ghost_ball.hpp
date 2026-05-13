#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::ghost_ball {

constexpr float kChaseSpeed = 1.1F;
constexpr float kChaseMaxSpeed = 1.0F;

extern const EntSpec kGhostBallSpec;

void StepEntLogicAsGhostBall(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntPhysicsAsGhostBall(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::ghost_ball

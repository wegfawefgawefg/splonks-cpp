#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::ghost_ball {

constexpr float kChaseSpeed = 1.1F;
constexpr float kChaseMaxSpeed = 1.0F;

extern const EntityArchetype kGhostBallArchetype;

void StepEntityLogicAsGhostBall(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntityPhysicsAsGhostBall(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::ghost_ball

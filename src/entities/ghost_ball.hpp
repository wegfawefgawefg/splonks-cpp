#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct State;

}

namespace splonks::entities::ghost_ball {

constexpr float kChaseSpeed = 1.1F;
constexpr float kChaseMaxSpeed = 1.0F;

extern const EntityArchetype kGhostBallArchetype;

void StepEntityLogicAsGhostBall(std::size_t entity_idx, State& state);
void StepEntityPhysicsAsGhostBall(std::size_t entity_idx, State& state, float dt);

} // namespace splonks::entities::ghost_ball

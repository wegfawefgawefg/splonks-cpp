#pragma once

#include "entity.hpp"

namespace splonks {

struct State;

}

namespace splonks::entities::ghost_ball {

constexpr float kChaseSpeed = 1.1F;
constexpr float kChaseMaxSpeed = 1.0F;

void SetEntityGhostBall(Entity& entity);
void StepEntityLogicAsGhostBall(std::size_t entity_idx, State& state);
void StepEntityPhysicsAsGhostBall(std::size_t entity_idx, State& state, float dt);

} // namespace splonks::entities::ghost_ball

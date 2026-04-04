#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::rope {

void SetEntityRope(Entity& entity);
void StepEntityLogicAsRope(std::size_t entity_idx, State& state, Audio& audio, Graphics& graphics);
void StepEntityPhysicsAsRope(std::size_t entity_idx, State& state, Audio& audio, float dt);

} // namespace splonks::entities::rope

#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::mouse_trailer {

void SetEntityMouseTrailer(Entity& entity);
void StepEntityLogicAsMouseTrailer(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsMouseTrailer(std::size_t entity_idx, State& state, float dt);

} // namespace splonks::entities::mouse_trailer

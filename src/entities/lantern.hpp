#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::lantern {

void SetEntityLantern(Entity& entity);
void SetEntityLanternRed(Entity& entity);
void StepEntityLogicAsLantern(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::lantern

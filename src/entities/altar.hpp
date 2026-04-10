#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::altar {

void SetEntityAltarLeft(Entity& entity);
void SetEntityAltarRight(Entity& entity);
void StepEntityLogicAsAltar(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::altar

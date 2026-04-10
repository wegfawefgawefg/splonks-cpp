#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::sac_altar {

void SetEntitySacAltarLeft(Entity& entity);
void SetEntitySacAltarRight(Entity& entity);
void StepEntityLogicAsSacAltar(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::sac_altar

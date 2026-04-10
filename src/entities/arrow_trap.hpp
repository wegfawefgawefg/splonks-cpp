#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::arrow_trap {

void SetEntityArrowTrap(Entity& entity);
void StepEntityLogicAsArrowTrap(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::arrow_trap

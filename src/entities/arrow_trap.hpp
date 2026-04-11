#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::arrow_trap {

extern const EntityArchetype kArrowTrapArchetype;

void StepEntityLogicAsArrowTrap(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::arrow_trap

#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::altar {

extern const EntityArchetype kAltarLeftArchetype;
extern const EntityArchetype kAltarRightArchetype;

void StepEntityLogicAsAltar(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::altar

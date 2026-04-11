#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::sac_altar {

extern const EntityArchetype kSacAltarLeftArchetype;
extern const EntityArchetype kSacAltarRightArchetype;

void StepEntityLogicAsSacAltar(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::sac_altar

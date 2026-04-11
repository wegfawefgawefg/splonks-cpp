#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::lantern {

extern const EntityArchetype kLanternArchetype;
extern const EntityArchetype kLanternRedArchetype;

void StepEntityLogicAsLantern(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::lantern

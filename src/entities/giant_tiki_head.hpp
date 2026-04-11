#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::giant_tiki_head {

extern const EntityArchetype kGiantTikiHeadArchetype;

void StepEntityLogicAsGiantTikiHead(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::giant_tiki_head

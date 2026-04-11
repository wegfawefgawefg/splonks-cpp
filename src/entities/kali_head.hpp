#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::kali_head {

extern const EntityArchetype kKaliHeadArchetype;

void StepEntityLogicAsKaliHead(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::kali_head

#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::giant_tiki_head {

extern const EntityArchetype kGiantTikiHeadArchetype;
void StepEntityLogicAsGiantTikiHead(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::giant_tiki_head

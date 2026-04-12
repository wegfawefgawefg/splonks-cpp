#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::rock {

extern const EntityArchetype kRockArchetype;

void StepEntityLogicAsRock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::rock

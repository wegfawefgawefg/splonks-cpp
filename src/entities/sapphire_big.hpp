#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::sapphire_big {

extern const EntityArchetype kSapphireBigArchetype;

void StepEntityLogicAsSapphireBig(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsSapphireBig(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::sapphire_big

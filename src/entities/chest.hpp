#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::chest {

extern const EntityArchetype kChestArchetype;

void StepEntityLogicAsChest(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsChest(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::chest

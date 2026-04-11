#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::shopkeeper {

extern const EntityArchetype kShopkeeperArchetype;

void StepEntityLogicAsShopkeeper(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsShopkeeper(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::shopkeeper

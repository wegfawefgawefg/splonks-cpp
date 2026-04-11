#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::gold_idol {

extern const EntityArchetype kGoldIdolArchetype;

void StepEntityLogicAsGoldIdol(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsGoldIdol(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::gold_idol

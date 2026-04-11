#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::emerald_big {

extern const EntityArchetype kEmeraldBigArchetype;

void StepEntityLogicAsEmeraldBig(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsEmeraldBig(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::emerald_big

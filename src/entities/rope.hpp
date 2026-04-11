#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::rope {

extern const EntityArchetype kRopeArchetype;

void StepEntityLogicAsRope(std::size_t entity_idx, State& state, Audio& audio, Graphics& graphics);
void StepEntityPhysicsAsRope(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::rope

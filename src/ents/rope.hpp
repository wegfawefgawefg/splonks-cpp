#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::rope {

extern const EntityArchetype kRopeArchetype;

void OnUseAsRope(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntityLogicAsRope(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::rope

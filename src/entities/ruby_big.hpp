#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::ruby_big {

extern const EntityArchetype kRubyBigArchetype;

void StepEntityLogicAsRubyBig(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsRubyBig(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::ruby_big

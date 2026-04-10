#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::ruby_big {

void SetEntityRubyBig(Entity& entity);
void StepEntityLogicAsRubyBig(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsRubyBig(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::ruby_big

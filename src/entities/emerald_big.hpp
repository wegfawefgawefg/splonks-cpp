#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::emerald_big {

void SetEntityEmeraldBig(Entity& entity);
void StepEntityLogicAsEmeraldBig(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsEmeraldBig(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::emerald_big

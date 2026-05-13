#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::flesh_guy {

extern const EntityArchetype kFleshGuyArchetype;

void OnDeathAsFleshGuy(std::size_t entity_idx, State& state, Audio& audio);
void ControlEntityAsFleshGuy(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntityLogicAsFleshGuy(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntityPhysicsAsFleshGuy(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::flesh_guy

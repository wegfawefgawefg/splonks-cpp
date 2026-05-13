#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::flesh_guy {

extern const EntSpec kFleshGuySpec;

void OnDeathAsFleshGuy(std::size_t ent_idx, State& state, Audio& audio);
void ControlEntAsFleshGuy(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntLogicAsFleshGuy(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntPhysicsAsFleshGuy(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::flesh_guy

#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::flappy_bee {

extern const EntSpec kFlappyBeeSpec;

void OnDeathAsFlappyBee(std::size_t ent_idx, State& state, Audio& audio);
void ControlEntAsFlappyBee(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntLogicAsFlappyBee(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntPhysicsAsFlappyBee(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::flappy_bee

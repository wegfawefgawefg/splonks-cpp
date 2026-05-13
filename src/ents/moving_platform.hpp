#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

} // namespace splonks

namespace splonks::ents::moving_platform {

extern const EntSpec kMovingPlatformSpec;

void StepEntLogicAsMovingPlatform(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntPhysicsAsMovingPlatform(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::moving_platform

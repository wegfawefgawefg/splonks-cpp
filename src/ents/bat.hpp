#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::bat {

constexpr float kChaseSpeed = 0.5F;
constexpr float kChaseMaxSpeed = 1.0F;

extern const EntSpec kBatSpec;

void StepEntLogicAsBat(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntPhysicsAsBat(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::bat

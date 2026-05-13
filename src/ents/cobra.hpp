#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

} // namespace splonks

namespace splonks::ents::cobra {

extern const EntSpec kCobraSpec;
extern const EntSpec kCobraSpitSpec;

void StepEntLogicAsCobra(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntLogicAsCobraSpit(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntPhysicsAsCobraSpit(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::cobra

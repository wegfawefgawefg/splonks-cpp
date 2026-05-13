#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

} // namespace splonks

namespace splonks::ents::dvdlogo {

extern const EntSpec kDvdLogoSpec;

void StepEntLogicAsDvdLogo(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::dvdlogo

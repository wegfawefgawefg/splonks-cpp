#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::mouse_trailer {

extern const EntSpec kMouseTrailerSpec;
void StepEntPhysicsAsMouseTrailer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::mouse_trailer

#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::rope {

extern const EntSpec kRopeSpec;

void OnUseAsRope(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntLogicAsRope(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::rope

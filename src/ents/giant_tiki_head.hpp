#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::giant_tiki_head {

extern const EntSpec kGiantTikiHeadSpec;
void StepEntLogicAsGiantTikiHead(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::giant_tiki_head

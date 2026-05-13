#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::bomb {

extern const EntSpec kBombSpec;

void MarkBombSticky(Ent& bomb);
void OnDeathAsBomb(std::size_t ent_idx, State& state, Audio& audio);
void OnUseAsBomb(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntLogicAsBomb(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
ents::common::ContactResult OnEntContactAsBomb(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const ents::common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
);
ents::common::ContactResult OnTileContactAsBomb(
    std::size_t ent_idx,
    const ents::common::ContactContext& context,
    State& state
);

} // namespace splonks::ents::bomb

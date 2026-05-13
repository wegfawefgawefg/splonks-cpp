#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::baseball_bat {

constexpr std::uint32_t kBatContactCooldownFrames = 9;

enum class SwingStage {
    Back,
    Above,
    Swing,
};

extern const EntSpec kBaseballBatSpec;

bool TryApplyBatContactToEnt(
    std::size_t bat_ent_idx,
    std::size_t other_ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void StepBaseballBat(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
bool IsStuff(EntType type_);

} // namespace splonks::ents::baseball_bat

#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::boulder {

extern const EntSpec kBoulderSpec;
void SpawnBoulderBreakEffects(const Vec2& center, State& state);
void OnDeathAsBoulder(std::size_t ent_idx, State& state, Audio& audio);
void StepEntLogicAsBoulder(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntPhysicsAsBoulder(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::boulder

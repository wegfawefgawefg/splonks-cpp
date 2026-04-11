#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::jetpack {

constexpr float kTravelSoundDistInterval = 8.0F;
constexpr float kFuel = 120.0F;

extern const EntityArchetype kJetPackArchetype;

void StepEntityLogicAsJetpack(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsJetpack(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::jetpack

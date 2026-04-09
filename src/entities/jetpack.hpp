#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::jetpack {

constexpr float kTravelSoundDistInterval = 8.0F;
constexpr float kFuel = 120.0F;

void SetEntityJetpack(Entity& entity);
void StepEntityLogicAsJetpack(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
void StepEntityPhysicsAsJetpack(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::jetpack

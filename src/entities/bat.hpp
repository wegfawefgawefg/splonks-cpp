#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::bat {

constexpr float kChaseSpeed = 0.5F;
constexpr float kChaseMaxSpeed = 1.0F;

extern const EntityArchetype kBatArchetype;

void StepEntityLogicAsBat(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsBat(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::bat

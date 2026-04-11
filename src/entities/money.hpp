#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::money {

constexpr IVec2 kSize = {8, 8};

extern const EntityArchetype kGoldArchetype;
extern const EntityArchetype kGoldStackArchetype;

void StepEntityLogicAsMoney(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsMoney(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::money

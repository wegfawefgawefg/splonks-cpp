#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::bomb {

extern const EntityArchetype kBombArchetype;

void OnDeathAsBomb(std::size_t entity_idx, State& state, Audio& audio);
void OnUseAsBomb(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntityLogicAsBomb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::bomb

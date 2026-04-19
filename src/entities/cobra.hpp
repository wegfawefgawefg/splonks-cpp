#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

} // namespace splonks

namespace splonks::entities::cobra {

extern const EntityArchetype kCobraArchetype;
extern const EntityArchetype kCobraSpitArchetype;

void StepEntityLogicAsCobra(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntityLogicAsCobraSpit(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntityPhysicsAsCobraSpit(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::cobra

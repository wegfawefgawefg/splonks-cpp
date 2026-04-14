#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

} // namespace splonks

namespace splonks::entities::dvdlogo {

extern const EntityArchetype kDvdLogoArchetype;

void StepEntityLogicAsDvdLogo(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::dvdlogo

#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::mouse_trailer {

extern const EntityArchetype kMouseTrailerArchetype;
void StepEntityPhysicsAsMouseTrailer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::mouse_trailer

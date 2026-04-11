#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::spider_hang {

extern const EntityArchetype kSpiderHangArchetype;
extern const EntityArchetype kGiantSpiderHangArchetype;

void StepEntityLogicAsSpiderHang(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsSpiderHang(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::spider_hang

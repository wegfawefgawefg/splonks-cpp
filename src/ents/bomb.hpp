#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::bomb {

extern const EntityArchetype kBombArchetype;

void MarkBombSticky(Entity& bomb);
void OnDeathAsBomb(std::size_t entity_idx, State& state, Audio& audio);
void OnUseAsBomb(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntityLogicAsBomb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
entities::common::ContactResolution OnEntityContactAsBomb(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const entities::common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
);
entities::common::ContactResolution OnTileContactAsBomb(
    std::size_t entity_idx,
    const entities::common::ContactContext& context,
    State& state
);

} // namespace splonks::entities::bomb

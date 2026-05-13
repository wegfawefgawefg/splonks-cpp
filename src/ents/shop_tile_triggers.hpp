#pragma once

#include "stage.hpp"

namespace splonks {

struct Audio;
struct State;

namespace ents::shop {

StageTileTrigger MakeShopVandalismTileTrigger(const IVec2& tile_pos, std::size_t target_spawn_index);
StageTileTrigger MakeShopVandalismTileTrigger(const IVec2& tile_pos, VID target_vid);

void OnShopVandalismTileDestroyed(
    const StageTileTrigger& trigger,
    const IVec2& tile_pos,
    State& state,
    Audio& audio
);

} // namespace ents::shop

} // namespace splonks

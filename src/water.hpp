#pragma once

#include "stage.hpp"
#include "tile.hpp"

namespace splonks {

bool IsWaterTile(Tile tile);
bool IsWaterSurfaceTile(const Stage& stage, const IVec2& tile_coord);
bool IsWaterAtWorldPos(const Stage& stage, const Vec2& world_pos);
bool IsWaterAtWorldPos(const Stage& stage, const Vec2& world_pos, float amount_cutoff);

} // namespace splonks

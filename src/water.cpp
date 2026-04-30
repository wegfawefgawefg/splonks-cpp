#include "water.hpp"

#include "world_query.hpp"

namespace splonks {

bool IsWaterTile(Tile tile) {
    return tile == Tile::WaterSwim || tile == Tile::WaterTop;
}

bool IsWaterSurfaceTile(const Stage& stage, const IVec2& tile_coord) {
    if (!stage.IsTileCoordInside(tile_coord.x, tile_coord.y)) {
        return false;
    }
    const auto x = static_cast<unsigned int>(tile_coord.x);
    const auto y = static_cast<unsigned int>(tile_coord.y);
    if (!IsWaterTile(stage.GetFluidTile(x, y)) && !IsWaterTile(stage.GetTile(x, y))) {
        return false;
    }

    const IVec2 above = tile_coord + IVec2::New(0, -1);
    if (!stage.IsTileCoordInside(above.x, above.y)) {
        return true;
    }
    const auto above_x = static_cast<unsigned int>(above.x);
    const auto above_y = static_cast<unsigned int>(above.y);
    return !IsWaterTile(stage.GetFluidTile(above_x, above_y)) &&
           !IsWaterTile(stage.GetTile(above_x, above_y));
}

bool IsWaterAtWorldPos(const Stage& stage, const Vec2& world_pos) {
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(stage, ToIVec2(world_pos));
    if (!tile_query.has_value()) {
        return false;
    }
    if (stage.IsTileCoordInside(tile_query->tile_pos.x, tile_query->tile_pos.y)) {
        const auto x = static_cast<unsigned int>(tile_query->tile_pos.x);
        const auto y = static_cast<unsigned int>(tile_query->tile_pos.y);
        if (IsWaterTile(stage.GetFluidTile(x, y))) {
            return true;
        }
    }
    return tile_query->tile != nullptr && IsWaterTile(*tile_query->tile);
}

} // namespace splonks

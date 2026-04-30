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
    if (!IsWaterTile(stage.GetTile(
            static_cast<unsigned int>(tile_coord.x),
            static_cast<unsigned int>(tile_coord.y)))) {
        return false;
    }

    const IVec2 above = tile_coord + IVec2::New(0, -1);
    if (!stage.IsTileCoordInside(above.x, above.y)) {
        return true;
    }
    return stage.GetTile(static_cast<unsigned int>(above.x), static_cast<unsigned int>(above.y)) == Tile::Air;
}

bool IsWaterAtWorldPos(const Stage& stage, const Vec2& world_pos) {
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(stage, ToIVec2(world_pos));
    return tile_query.has_value() && tile_query->tile != nullptr && IsWaterTile(*tile_query->tile);
}

} // namespace splonks

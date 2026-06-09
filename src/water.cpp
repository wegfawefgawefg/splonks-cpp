#include "water.hpp"

#include "world_query.hpp"

namespace splonks {

bool IsWaterTile(Tile tile) {
    return tile == Tile::WaterSwim || tile == Tile::WaterTop;
}

bool IsWaterSurfaceTile(const Stage& stage, const IVec2& tile_coord) {
    const IVec2 wrapped_tile_coord = stage.WrapTileCoord(tile_coord);
    if (!stage.IsTileCoordInside(wrapped_tile_coord.x, wrapped_tile_coord.y)) {
        return false;
    }
    const auto x = static_cast<unsigned int>(wrapped_tile_coord.x);
    const auto y = static_cast<unsigned int>(wrapped_tile_coord.y);
    const bool has_fluid_water = IsWaterTile(stage.GetFluidTile(x, y)) &&
                                 stage.GetFluidAmount(x, y) > 0;
    if (!has_fluid_water && !IsWaterTile(stage.GetTile(x, y))) {
        return false;
    }

    const IVec2 above = stage.WrapTileCoord(wrapped_tile_coord + IVec2::New(0, -1));
    if (!stage.IsTileCoordInside(above.x, above.y)) {
        return true;
    }
    const auto above_x = static_cast<unsigned int>(above.x);
    const auto above_y = static_cast<unsigned int>(above.y);
    const bool has_water_above = IsWaterTile(stage.GetFluidTile(above_x, above_y)) &&
                                 stage.GetFluidAmount(above_x, above_y) > 0;
    return !has_water_above &&
           !IsWaterTile(stage.GetTile(above_x, above_y));
}

bool IsWaterAtWorldPos(const Stage& stage, const FVec2& world_pos, float amount_cutoff) {
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(stage, ToIVec2(world_pos));
    if (!tile_query.has_value()) {
        return false;
    }
    if (stage.IsTileCoordInside(tile_query->tile_pos.x, tile_query->tile_pos.y)) {
        const auto x = static_cast<unsigned int>(tile_query->tile_pos.x);
        const auto y = static_cast<unsigned int>(tile_query->tile_pos.y);
        if (IsWaterTile(stage.GetFluidTile(x, y)) && stage.GetFluidAmount(x, y) >= amount_cutoff) {
            return true;
        }
    }
    return tile_query->tile != nullptr && IsWaterTile(*tile_query->tile);
}

bool IsWaterAtWorldPos(const Stage& stage, const FVec2& world_pos) {
    return IsWaterAtWorldPos(stage, world_pos, 0.0F);
}

bool IsWaterAtWorldPos(const Stage& stage, sim::FxVec2 world_pos, float amount_cutoff) {
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(stage, world_pos);
    if (!tile_query.has_value()) {
        return false;
    }
    if (stage.IsTileCoordInside(tile_query->tile_pos.x, tile_query->tile_pos.y)) {
        const auto x = static_cast<unsigned int>(tile_query->tile_pos.x);
        const auto y = static_cast<unsigned int>(tile_query->tile_pos.y);
        if (IsWaterTile(stage.GetFluidTile(x, y)) && stage.GetFluidAmount(x, y) >= amount_cutoff) {
            return true;
        }
    }
    return tile_query->tile != nullptr && IsWaterTile(*tile_query->tile);
}

bool IsWaterAtWorldPos(const Stage& stage, sim::FxVec2 world_pos) {
    return IsWaterAtWorldPos(stage, world_pos, 0.0F);
}

} // namespace splonks

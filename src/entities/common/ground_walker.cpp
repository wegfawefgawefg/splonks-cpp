#include "entities/common/ground_walker.hpp"

#include "tile.hpp"
#include "world_query.hpp"

namespace splonks::entities::common {

bool IsSolidTileAtWorldPos(const State& state, const IVec2& world_pos) {
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, world_pos);
    return tile_query.has_value() && tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile);
}

bool HasWallAheadForGroundWalker(const Entity& entity, const State& state, int direction) {
    const auto [tl, br] = entity.GetBounds();
    const int sample_x = direction < 0 ? static_cast<int>(tl.x) - 1 : static_cast<int>(br.x) + 1;
    const int sample_y_top = static_cast<int>(tl.y) + 1;
    const int sample_y_bottom = static_cast<int>(br.y) - 1;
    return IsSolidTileAtWorldPos(state, IVec2::New(sample_x, sample_y_top)) ||
           IsSolidTileAtWorldPos(state, IVec2::New(sample_x, sample_y_bottom));
}

bool HasGroundAheadForGroundWalker(const Entity& entity, const State& state, int direction) {
    const auto [tl, br] = entity.GetBounds();
    const int sample_x = direction < 0 ? static_cast<int>(tl.x) - 1 : static_cast<int>(br.x) + 1;
    const int sample_y = static_cast<int>(br.y) + 1;
    return IsSolidTileAtWorldPos(state, IVec2::New(sample_x, sample_y));
}

} // namespace splonks::entities::common

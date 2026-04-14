#include "entities/common/ground_walker.hpp"

#include "tile.hpp"
#include "world_query.hpp"

namespace splonks::entities::common {

bool IsSolidTileAtWorldPos(const State& state, const IVec2& world_pos) {
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, world_pos);
    return tile_query.has_value() && tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile);
}

bool HasWallAheadForGroundWalker(
    const Entity& entity,
    const State& state,
    const Graphics& graphics,
    int direction
) {
    const auto [tl, br] = entity.GetBounds();
    const float sample_x = direction < 0 ? tl.x - 1.0F : br.x + 1.0F;
    const AABB probe = AABB::New(
        Vec2::New(sample_x, tl.y + 1.0F),
        Vec2::New(sample_x, br.y - 1.0F)
    );
    return AabbHitsBlockingWorldGeometryOrImpassableEntities(state, graphics, probe, entity.vid);
}

bool HasGroundAheadForGroundWalker(
    const Entity& entity,
    const State& state,
    const Graphics& graphics,
    int direction
) {
    const auto [tl, br] = entity.GetBounds();
    const float sample_x = direction < 0 ? tl.x - 1.0F : br.x + 1.0F;
    const float sample_y = br.y + 1.0F;
    const AABB probe = AABB::New(
        Vec2::New(sample_x, sample_y),
        Vec2::New(sample_x, sample_y)
    );
    return AabbHitsBlockingWorldGeometryOrImpassableEntities(state, graphics, probe, entity.vid);
}

} // namespace splonks::entities::common

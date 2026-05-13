#include "ents/common/ground_walker.hpp"

#include "tile.hpp"
#include "world_query.hpp"

namespace splonks::ents::common {

bool IsSolidTileAtWorldPos(const State& state, const IVec2& world_pos) {
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, world_pos);
    return tile_query.has_value() && tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile);
}

bool HasWallAheadForGroundWalker(
    const Ent& ent,
    const State& state,
    const Graphics& graphics,
    int direction
) {
    const auto [tl, br] = ent.GetBounds();
    const float sample_x = direction < 0 ? tl.x - 1.0F : br.x + 1.0F;
    const AABB probe = AABB::New(
        Vec2::New(sample_x, tl.y + 1.0F),
        Vec2::New(sample_x, br.y - 1.0F)
    );
    return AabbHitsBlockingWorldGeometryOrImpassableEnts(state, graphics, probe, ent.vid);
}

bool HasGroundAheadForGroundWalker(
    const Ent& ent,
    const State& state,
    const Graphics& graphics,
    int direction
) {
    const auto [tl, br] = ent.GetBounds();
    const float sample_x = direction < 0 ? tl.x - 1.0F : br.x + 1.0F;
    const float sample_y = br.y + 1.0F;
    const AABB probe = AABB::New(
        Vec2::New(sample_x, sample_y),
        Vec2::New(sample_x, sample_y)
    );
    return AabbHitsBlockingWorldGeometryOrImpassableEnts(state, graphics, probe, ent.vid);
}

} // namespace splonks::ents::common

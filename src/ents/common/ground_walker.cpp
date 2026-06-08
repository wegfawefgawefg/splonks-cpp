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
    const sim::AABB bounds = ent.GetSimAABB();
    const sim::Scalar sample_x = direction < 0
        ? bounds.tl.x - sim::Scalar::from_int(1)
        : bounds.br.x + sim::Scalar::from_int(1);
    const sim::AABB probe = sim::AABB::from_corners(
        sim::Vec2{sample_x, bounds.tl.y + sim::Scalar::from_int(1)},
        sim::Vec2{sample_x, bounds.br.y - sim::Scalar::from_int(1)}
    );
    return AabbHitsBlockingWorldGeometryOrImpassableEnts(state, graphics, probe, ent.vid);
}

bool HasGroundAheadForGroundWalker(
    const Ent& ent,
    const State& state,
    const Graphics& graphics,
    int direction
) {
    const sim::AABB bounds = ent.GetSimAABB();
    const sim::Scalar sample_x = direction < 0
        ? bounds.tl.x - sim::Scalar::from_int(1)
        : bounds.br.x + sim::Scalar::from_int(1);
    const sim::Scalar sample_y = bounds.br.y + sim::Scalar::from_int(1);
    const sim::AABB probe = sim::AABB::from_corners(
        sim::Vec2{sample_x, sample_y},
        sim::Vec2{sample_x, sample_y}
    );
    return AabbHitsBlockingWorldGeometryOrImpassableEnts(state, graphics, probe, ent.vid);
}

} // namespace splonks::ents::common

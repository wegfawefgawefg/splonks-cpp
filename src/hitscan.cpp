#include "hitscan.hpp"

#include "world_query.hpp"

namespace splonks {

HitscanHit TraceHitscan(
    const Ent& source_ent,
    sim::Vec2 start_pos,
    int direction,
    int max_distance,
    State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    const WorldRayHit hit = RaycastHorizontal(
        source_ent,
        start_pos,
        direction,
        max_distance,
        state,
        graphics,
        owner_vid
    );

    HitscanHit result;
    result.point = hit.point;
    result.ent_vid = hit.ent_vid;
    switch (hit.type) {
    case WorldRayHitType::None:
        result.type = HitscanHitType::None;
        break;
    case WorldRayHitType::StageBounds:
        result.type = HitscanHitType::StageBounds;
        break;
    case WorldRayHitType::Tile:
        result.type = HitscanHitType::Tile;
        break;
    case WorldRayHitType::Ent:
        result.type = HitscanHitType::Ent;
        break;
    }

    return result;
}

} // namespace splonks

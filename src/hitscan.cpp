#include "hitscan.hpp"

#include "world_query.hpp"

namespace splonks {

HitscanHit TraceHitscan(
    const Entity& source_entity,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    const WorldRayHit hit = RaycastHorizontal(
        source_entity,
        start_pos,
        direction,
        max_distance,
        state,
        graphics,
        owner_vid
    );

    HitscanHit result;
    result.point = hit.point;
    result.entity_vid = hit.entity_vid;
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
    case WorldRayHitType::Entity:
        result.type = HitscanHitType::Entity;
        break;
    }

    return result;
}

} // namespace splonks

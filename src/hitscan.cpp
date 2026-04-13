#include "hitscan.hpp"

#include "entities/common/common.hpp"
#include "tile.hpp"

#include <vector>

namespace splonks {

namespace {

struct HitscanTarget {
    VID vid;
    AABB aabb;
};

bool PointInAabb(const IVec2& point, const AABB& aabb) {
    return static_cast<float>(point.x) >= aabb.tl.x &&
           static_cast<float>(point.x) <= aabb.br.x &&
           static_cast<float>(point.y) >= aabb.tl.y &&
           static_cast<float>(point.y) <= aabb.br.y;
}

std::vector<HitscanTarget> CollectHitscanTargets(
    const Entity& source_entity,
    const std::optional<VID>& owner_vid,
    State& state,
    const Graphics& graphics,
    const AABB& ray_aabb
) {
    const std::vector<VIDAABB> broadphase_hits =
        state.sid.QueryForVIDAABBsExclude(ray_aabb.tl, ray_aabb.br, source_entity.vid);

    std::vector<HitscanTarget> targets;
    targets.reserve(broadphase_hits.size());
    for (const VIDAABB& hit : broadphase_hits) {
        if (owner_vid.has_value() && hit.vid == *owner_vid) {
            continue;
        }

        const Entity* const entity = state.entity_manager.GetEntity(hit.vid);
        if (entity == nullptr || !entity->active) {
            continue;
        }
        if (owner_vid.has_value() && entity->held_by_vid.has_value() &&
            entity->held_by_vid == owner_vid) {
            continue;
        }

        targets.push_back(HitscanTarget{
            .vid = hit.vid,
            .aabb = entities::common::GetContactAabbForEntity(*entity, graphics),
        });
    }

    return targets;
}

} // namespace

HitscanHit TraceHitscan(
    const Entity& source_entity,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    const int ray_y = static_cast<int>(start_pos.y);
    const int start_x = static_cast<int>(start_pos.x);
    const int end_x = start_x + (direction * max_distance);
    const AABB ray_aabb = AABB::New(
        Vec2::New(static_cast<float>(start_x < end_x ? start_x : end_x), static_cast<float>(ray_y)),
        Vec2::New(static_cast<float>(start_x > end_x ? start_x : end_x), static_cast<float>(ray_y))
    );
    const std::vector<HitscanTarget> targets =
        CollectHitscanTargets(source_entity, owner_vid, state, graphics, ray_aabb);

    for (int step = 0; step < max_distance; ++step) {
        const IVec2 point = IVec2::New(start_x + (direction * step), ray_y);
        if (!state.stage.TileCoordAtWcExists(point)) {
            return HitscanHit{
                .type = HitscanHitType::StageBounds,
                .point = point,
                .entity_vid = std::nullopt,
            };
        }

        const Tile* const tile = state.stage.GetTileAtWc(point);
        if (tile != nullptr && IsTileCollidable(*tile)) {
            return HitscanHit{
                .type = HitscanHitType::Tile,
                .point = point,
                .entity_vid = std::nullopt,
            };
        }

        for (const HitscanTarget& target : targets) {
            if (!PointInAabb(point, target.aabb)) {
                continue;
            }

            return HitscanHit{
                .type = HitscanHitType::Entity,
                .point = point,
                .entity_vid = target.vid,
            };
        }
    }

    return HitscanHit{};
}

} // namespace splonks

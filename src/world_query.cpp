#include "world_query.hpp"

#include "entities/common/common.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace splonks {

namespace {

float GetNearestWrappedDelta(float from, float to, float span, bool wraps) {
    float delta = to - from;
    if (!wraps || span <= 0.0F) {
        return delta;
    }

    delta = std::fmod(delta, span);
    if (delta > span * 0.5F) {
        delta -= span;
    }
    if (delta < -span * 0.5F) {
        delta += span;
    }
    return delta;
}

AABB ShiftAabb(const AABB& aabb, const Vec2& delta) {
    return AABB{
        .tl = aabb.tl + delta,
        .br = aabb.br + delta,
    };
}

Vec2 GetAabbCenter(const AABB& aabb) {
    return (aabb.tl + aabb.br) / 2.0F;
}

int FloorDivBySpan(float value, float span) {
    if (span <= 0.0F) {
        return 0;
    }
    return static_cast<int>(std::floor(value / span));
}

std::vector<Vec2> GetQueryOffsets(const Stage& stage, const AABB& area) {
    std::vector<Vec2> offsets;
    offsets.push_back(Vec2::New(0.0F, 0.0F));

    const float stage_width = static_cast<float>(stage.GetWidth());
    const float stage_height = static_cast<float>(stage.GetHeight());
    if ((!stage.WrapsX() || stage_width <= 0.0F) && (!stage.WrapsY() || stage_height <= 0.0F)) {
        return offsets;
    }

    const int min_copy_x = stage.WrapsX() ? FloorDivBySpan(area.tl.x, stage_width) : 0;
    const int max_copy_x = stage.WrapsX() ? FloorDivBySpan(area.br.x, stage_width) : 0;
    const int min_copy_y = stage.WrapsY() ? FloorDivBySpan(area.tl.y, stage_height) : 0;
    const int max_copy_y = stage.WrapsY() ? FloorDivBySpan(area.br.y, stage_height) : 0;

    offsets.clear();
    for (int copy_y = min_copy_y; copy_y <= max_copy_y; ++copy_y) {
        for (int copy_x = min_copy_x; copy_x <= max_copy_x; ++copy_x) {
            offsets.push_back(Vec2::New(
                static_cast<float>(copy_x) * stage_width,
                static_cast<float>(copy_y) * stage_height
            ));
        }
    }
    return offsets;
}

bool PointInAabb(const IVec2& point, const AABB& aabb) {
    return static_cast<float>(point.x) >= aabb.tl.x &&
           static_cast<float>(point.x) <= aabb.br.x &&
           static_cast<float>(point.y) >= aabb.tl.y &&
           static_cast<float>(point.y) <= aabb.br.y;
}

} // namespace

Vec2 GetNearestWorldDelta(const Stage& stage, const Vec2& from, const Vec2& to) {
    return Vec2::New(
        GetNearestWrappedDelta(from.x, to.x, static_cast<float>(stage.GetWidth()), stage.WrapsX()),
        GetNearestWrappedDelta(from.y, to.y, static_cast<float>(stage.GetHeight()), stage.WrapsY())
    );
}

Vec2 GetNearestWorldPoint(const Stage& stage, const Vec2& anchor, const Vec2& point) {
    return anchor + GetNearestWorldDelta(stage, anchor, point);
}

AABB GetNearestWorldAabb(const Stage& stage, const Vec2& anchor, const AABB& aabb) {
    const Vec2 center = GetAabbCenter(aabb);
    const Vec2 nearest_center = GetNearestWorldPoint(stage, anchor, center);
    return ShiftAabb(aabb, nearest_center - center);
}

bool WorldAabbContainsPoint(const Stage& stage, const AABB& area, const Vec2& point) {
    const AABB nearest_area = GetNearestWorldAabb(stage, point, area);
    return point.x >= nearest_area.tl.x && point.x <= nearest_area.br.x &&
           point.y >= nearest_area.tl.y && point.y <= nearest_area.br.y;
}

bool WorldAabbsIntersect(const Stage& stage, const AABB& area, const AABB& other) {
    const Vec2 anchor = GetAabbCenter(area);
    const AABB nearest_other = GetNearestWorldAabb(stage, anchor, other);
    return AabbsIntersect(area, nearest_other);
}

std::vector<IVec2> GetTileCoordsInRect(const Stage& stage, const IVec2& tl, const IVec2& br) {
    std::vector<IVec2> result;
    const unsigned int tile_width = stage.GetTileWidth();
    const unsigned int tile_height = stage.GetTileHeight();
    if (tile_width == 0 || tile_height == 0) {
        return result;
    }

    std::vector<bool> visited(static_cast<std::size_t>(tile_width * tile_height), false);
    for (int y = tl.y; y <= br.y; ++y) {
        for (int x = tl.x; x <= br.x; ++x) {
            const IVec2 tile_pos = stage.WrapTileCoord(IVec2::New(x, y));
            if (!stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                continue;
            }
            const std::size_t idx = static_cast<std::size_t>(tile_pos.y) * tile_width +
                                    static_cast<std::size_t>(tile_pos.x);
            if (visited[idx]) {
                continue;
            }
            visited[idx] = true;
            result.push_back(tile_pos);
        }
    }
    return result;
}

std::vector<WorldTileQueryResult> QueryTilesInRect(
    const Stage& stage,
    const IVec2& tl,
    const IVec2& br
) {
    std::vector<WorldTileQueryResult> result;
    for (const IVec2& tile_pos : GetTileCoordsInRect(stage, tl, br)) {
        result.push_back(WorldTileQueryResult{
            .tile_pos = tile_pos,
            .tile = &stage.GetTile(
                static_cast<unsigned int>(tile_pos.x),
                static_cast<unsigned int>(tile_pos.y)
            ),
        });
    }
    return result;
}

std::vector<WorldTileQueryResult> QueryTilesInWorldRect(
    const Stage& stage,
    const IVec2& tl,
    const IVec2& br
) {
    return QueryTilesInRect(
        stage,
        IVec2::New(
            FloorDivBySpan(static_cast<float>(tl.x), static_cast<float>(kTileSize)),
            FloorDivBySpan(static_cast<float>(tl.y), static_cast<float>(kTileSize))
        ),
        IVec2::New(
            FloorDivBySpan(static_cast<float>(br.x), static_cast<float>(kTileSize)),
            FloorDivBySpan(static_cast<float>(br.y), static_cast<float>(kTileSize))
        )
    );
}

std::vector<WorldTileQueryResult> QueryTilesInAabb(const Stage& stage, const AABB& area) {
    return QueryTilesInWorldRect(stage, ToIVec2(area.tl), ToIVec2(area.br));
}

bool AabbTouchesBlockingStageBounds(const Stage& stage, const AABB& area) {
    if (area.tl.x < 0.0F && stage.IsBorderSideBlocking(StageBorderSideKind::Left)) {
        return true;
    }
    if (area.tl.y < 0.0F && stage.IsBorderSideBlocking(StageBorderSideKind::Top)) {
        return true;
    }
    if (area.br.x > static_cast<float>(stage.GetWidth() - 1) &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Right)) {
        return true;
    }
    if (area.br.y > static_cast<float>(stage.GetHeight() - 1) &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Bottom)) {
        return true;
    }
    return false;
}

bool AabbHitsBlockingTiles(const Stage& stage, const AABB& area) {
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(stage, area)) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return true;
        }
    }
    return false;
}

bool AabbHitsBlockingWorldGeometry(const Stage& stage, const AABB& area) {
    return AabbTouchesBlockingStageBounds(stage, area) || AabbHitsBlockingTiles(stage, area);
}

bool AabbHitsImpassableEntities(
    const State& state,
    const Graphics& graphics,
    const AABB& area,
    std::optional<VID> exclude_vid
) {
    const Vec2 anchor = (area.tl + area.br) / 2.0F;
    for (const VID& vid : QueryEntitiesInAabb(state, area, exclude_vid)) {
        const Entity* const entity = state.entity_manager.GetEntity(vid);
        if (entity == nullptr || !entity->active || !entity->impassable) {
            continue;
        }

        const AABB entity_aabb = GetNearestWorldAabb(
            state.stage,
            anchor,
            entities::common::GetContactAabbForEntity(*entity, graphics)
        );
        if (AabbsIntersect(area, entity_aabb)) {
            return true;
        }
    }
    return false;
}

bool AabbHitsBlockingWorldGeometryOrImpassableEntities(
    const State& state,
    const Graphics& graphics,
    const AABB& area,
    std::optional<VID> exclude_vid
) {
    return AabbHitsBlockingWorldGeometry(state.stage, area) ||
           AabbHitsImpassableEntities(state, graphics, area, exclude_vid);
}

std::optional<WorldTileQueryResult> QueryTileAtTilePos(const Stage& stage, const IVec2& tile_pos) {
    const IVec2 wrapped = stage.WrapTileCoord(tile_pos);
    if (!stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
        return std::nullopt;
    }
    return WorldTileQueryResult{
        .tile_pos = wrapped,
        .tile = &stage.GetTile(static_cast<unsigned int>(wrapped.x), static_cast<unsigned int>(wrapped.y)),
    };
}

std::optional<WorldTileQueryResult> QueryTileAtWorldPos(const Stage& stage, const IVec2& world_pos) {
    if (!stage.TileCoordAtWcExists(world_pos)) {
        return std::nullopt;
    }
    return QueryTileAtTilePos(stage, stage.GetTileCoordAtWc(world_pos));
}

std::vector<VID> QueryEntitiesInAabb(
    const State& state,
    const AABB& area,
    std::optional<VID> exclude_vid
) {
    std::vector<VID> result;
    std::vector<bool> seen(state.entity_manager.entities.size(), false);
    const std::vector<Vec2> offsets = GetQueryOffsets(state.stage, area);

    for (const Vec2& offset : offsets) {
        const AABB sample_area = AABB{
            .tl = area.tl - offset,
            .br = area.br - offset,
        };
        const std::vector<VID> hits = state.sid.Query(sample_area.tl, sample_area.br);
        for (const VID& vid : hits) {
            if (exclude_vid.has_value() && vid == *exclude_vid) {
                continue;
            }
            if (vid.id >= seen.size() || seen[vid.id]) {
                continue;
            }
            seen[vid.id] = true;
            result.push_back(vid);
        }
    }

    return result;
}

struct RaycastTarget {
    VID vid;
    AABB aabb;
};

std::vector<RaycastTarget> CollectRaycastTargets(
    const Entity& source_entity,
    const Vec2& start_pos,
    const AABB& ray_aabb,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    const std::vector<VID> hits = QueryEntitiesInAabb(state, ray_aabb, source_entity.vid);

    std::vector<RaycastTarget> targets;
    targets.reserve(hits.size());
    for (const VID& vid : hits) {
        if (owner_vid.has_value() && vid == *owner_vid) {
            continue;
        }

        const Entity* const entity = state.entity_manager.GetEntity(vid);
        if (entity == nullptr || !entity->active) {
            continue;
        }
        if (!entity->can_be_hit) {
            continue;
        }
        if (owner_vid.has_value() && entity->held_by_vid.has_value() &&
            entity->held_by_vid == owner_vid) {
            continue;
        }

        targets.push_back(RaycastTarget{
            .vid = vid,
            .aabb = GetNearestWorldAabb(
                state.stage,
                start_pos,
                entities::common::GetContactAabbForEntity(*entity, graphics)
            ),
        });
    }

    return targets;
}

std::optional<WorldRayHit> QueryEntityRayHitAtPoint(
    const IVec2& point,
    const std::vector<RaycastTarget>& targets
) {
    for (const RaycastTarget& target : targets) {
        if (!PointInAabb(point, target.aabb)) {
            continue;
        }
        return WorldRayHit{
            .type = WorldRayHitType::Entity,
            .point = point,
            .entity_vid = target.vid,
        };
    }
    return std::nullopt;
}

WorldRayHit QueryWorldRayHitAtPoint(
    const IVec2& point,
    const State& state,
    const std::vector<RaycastTarget>& targets
) {
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, point);
    if (!tile_query.has_value()) {
        return WorldRayHit{
            .type = WorldRayHitType::StageBounds,
            .point = point,
        };
    }

    if (tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile)) {
        return WorldRayHit{
            .type = WorldRayHitType::Tile,
            .point = point,
            .tile_pos = tile_query->tile_pos,
        };
    }

    if (const std::optional<WorldRayHit> entity_hit = QueryEntityRayHitAtPoint(point, targets)) {
        return *entity_hit;
    }

    return WorldRayHit{};
}

WorldRayHit RaycastTiles(
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state
) {
    const Vec2 step_dir = NormalizeOrZero(direction);
    if (max_distance <= 0 || step_dir == Vec2::New(0.0F, 0.0F)) {
        return WorldRayHit{};
    }

    for (int step = 0; step < max_distance; ++step) {
        const IVec2 point = ToIVec2(start_pos + (step_dir * static_cast<float>(step)));
        const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, point);
        if (!tile_query.has_value()) {
            return WorldRayHit{
                .type = WorldRayHitType::StageBounds,
                .point = point,
            };
        }

        if (tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile)) {
            return WorldRayHit{
                .type = WorldRayHitType::Tile,
                .point = point,
                .tile_pos = tile_query->tile_pos,
            };
        }
    }

    return WorldRayHit{};
}

WorldRayHit RaycastHorizontal(
    const Entity& source_entity,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    if (max_distance <= 0 || direction == 0) {
        return WorldRayHit{};
    }

    const int step_dir = direction < 0 ? -1 : 1;
    const int start_x = ToIVec2(start_pos).x;
    const int ray_y = ToIVec2(start_pos).y;
    const int end_x = start_x + (step_dir * max_distance);
    const AABB ray_aabb = AABB::New(
        Vec2::New(static_cast<float>(std::min(start_x, end_x)), static_cast<float>(ray_y)),
        Vec2::New(static_cast<float>(std::max(start_x, end_x)), static_cast<float>(ray_y))
    );
    const std::vector<RaycastTarget> targets =
        CollectRaycastTargets(source_entity, start_pos, ray_aabb, state, graphics, owner_vid);

    for (int step = 0; step < max_distance; ++step) {
        const IVec2 point = IVec2::New(start_x + (step_dir * step), ray_y);
        const WorldRayHit hit = QueryWorldRayHitAtPoint(point, state, targets);
        if (hit.type != WorldRayHitType::None) {
            return hit;
        }
    }

    return WorldRayHit{};
}

WorldRayHit RaycastVertical(
    const Entity& source_entity,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    if (max_distance <= 0 || direction == 0) {
        return WorldRayHit{};
    }

    const int step_dir = direction < 0 ? -1 : 1;
    const int ray_x = ToIVec2(start_pos).x;
    const int start_y = ToIVec2(start_pos).y;
    const int end_y = start_y + (step_dir * max_distance);
    const AABB ray_aabb = AABB::New(
        Vec2::New(static_cast<float>(ray_x), static_cast<float>(std::min(start_y, end_y))),
        Vec2::New(static_cast<float>(ray_x), static_cast<float>(std::max(start_y, end_y)))
    );
    const std::vector<RaycastTarget> targets =
        CollectRaycastTargets(source_entity, start_pos, ray_aabb, state, graphics, owner_vid);

    for (int step = 0; step < max_distance; ++step) {
        const IVec2 point = IVec2::New(ray_x, start_y + (step_dir * step));
        const WorldRayHit hit = QueryWorldRayHitAtPoint(point, state, targets);
        if (hit.type != WorldRayHitType::None) {
            return hit;
        }
    }

    return WorldRayHit{};
}

WorldRayHit RaycastEntities(
    const Entity& source_entity,
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    const Vec2 step_dir = NormalizeOrZero(direction);
    if (max_distance <= 0 || step_dir == Vec2::New(0.0F, 0.0F)) {
        return WorldRayHit{};
    }

    const Vec2 end_pos = start_pos + (step_dir * static_cast<float>(max_distance));
    const AABB ray_aabb = AABB::New(
        Vec2::New(std::min(start_pos.x, end_pos.x), std::min(start_pos.y, end_pos.y)),
        Vec2::New(std::max(start_pos.x, end_pos.x), std::max(start_pos.y, end_pos.y))
    );
    const std::vector<RaycastTarget> targets =
        CollectRaycastTargets(source_entity, start_pos, ray_aabb, state, graphics, owner_vid);

    for (int step = 0; step < max_distance; ++step) {
        const IVec2 point = ToIVec2(start_pos + (step_dir * static_cast<float>(step)));
        if (const std::optional<WorldRayHit> entity_hit = QueryEntityRayHitAtPoint(point, targets)) {
            return *entity_hit;
        }
    }

    return WorldRayHit{};
}

WorldRayHit RaycastWorld(
    const Entity& source_entity,
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    if (direction.x != 0.0F && direction.y == 0.0F) {
        return RaycastHorizontal(
            source_entity,
            start_pos,
            direction.x < 0.0F ? -1 : 1,
            max_distance,
            state,
            graphics,
            owner_vid
        );
    }
    if (direction.y != 0.0F && direction.x == 0.0F) {
        return RaycastVertical(
            source_entity,
            start_pos,
            direction.y < 0.0F ? -1 : 1,
            max_distance,
            state,
            graphics,
            owner_vid
        );
    }

    const Vec2 step_dir = NormalizeOrZero(direction);
    if (max_distance <= 0 || step_dir == Vec2::New(0.0F, 0.0F)) {
        return WorldRayHit{};
    }

    const Vec2 end_pos = start_pos + (step_dir * static_cast<float>(max_distance));
    const AABB ray_aabb = AABB::New(
        Vec2::New(std::min(start_pos.x, end_pos.x), std::min(start_pos.y, end_pos.y)),
        Vec2::New(std::max(start_pos.x, end_pos.x), std::max(start_pos.y, end_pos.y))
    );
    const std::vector<RaycastTarget> targets =
        CollectRaycastTargets(source_entity, start_pos, ray_aabb, state, graphics, owner_vid);

    for (int step = 0; step < max_distance; ++step) {
        const IVec2 point = ToIVec2(start_pos + (step_dir * static_cast<float>(step)));
        const WorldRayHit hit = QueryWorldRayHitAtPoint(point, state, targets);
        if (hit.type != WorldRayHitType::None) {
            return hit;
        }
    }

    return WorldRayHit{};
}

} // namespace splonks

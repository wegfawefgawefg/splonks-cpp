#include "world_query.hpp"

#include "ents/common/common.hpp"
#include "state.hpp"
#include "tile_spec.hpp"


#include <algorithm>
#include <vector>

namespace splonks {

namespace {

bool VidLess(const VID& left, const VID& right) {
    if (left.id != right.id) {
        return left.id < right.id;
    }
    return left.version < right.version;
}

float GetNearestWrappedDelta(float from, float to, float span, bool wraps) {
    float delta = to - from;
    if (!wraps || span <= 0.0F) {
        return delta;
    }

    delta -= static_cast<float>(static_cast<int>(delta / span)) * span;
    if (delta > span * 0.5F) {
        delta -= span;
    }
    if (delta < -span * 0.5F) {
        delta += span;
    }
    return delta;
}

FxScalar GetNearestWrappedDelta(FxScalar from,
                                   FxScalar to,
                                   FxScalar span,
                                   bool wraps) {
    FxScalar delta = to - from;
    if (!wraps || span <= FxScalar::zero()) {
        return delta;
    }

    const int copy = delta.trunc_int() / span.trunc_int();
    delta -= span * copy;
    if (delta > span / 2) {
        delta -= span;
    }
    if (delta < -(span / 2)) {
        delta += span;
    }
    return delta;
}

FxAABB ShiftAabb(FxAABB aabb, FxVec2 delta) {
    aabb.translate(delta);
    return aabb;
}

std::vector<FxVec2> GetQueryOffsets(const Stage& stage, FxAABB area) {
    std::vector<FxVec2> offsets;
    offsets.push_back(FxVec2::zero());

    const int stage_width_pixels = static_cast<int>(stage.GetWidth());
    const int stage_height_pixels = static_cast<int>(stage.GetHeight());
    const FxScalar stage_width = FxScalar::from_int(stage_width_pixels);
    const FxScalar stage_height = FxScalar::from_int(stage_height_pixels);
    if ((!stage.WrapsX() || stage_width <= FxScalar::zero()) &&
        (!stage.WrapsY() || stage_height <= FxScalar::zero())) {
        return offsets;
    }

    const int min_copy_x =
        stage.WrapsX() ? FloorDiv(area.tl.x.floor_int(), stage_width_pixels) : 0;
    const int max_copy_x =
        stage.WrapsX() ? FloorDiv(area.br.x.floor_int(), stage_width_pixels) : 0;
    const int min_copy_y =
        stage.WrapsY() ? FloorDiv(area.tl.y.floor_int(), stage_height_pixels) : 0;
    const int max_copy_y =
        stage.WrapsY() ? FloorDiv(area.br.y.floor_int(), stage_height_pixels) : 0;

    offsets.clear();
    for (int copy_y = min_copy_y; copy_y <= max_copy_y; ++copy_y) {
        for (int copy_x = min_copy_x; copy_x <= max_copy_x; ++copy_x) {
            offsets.push_back(FxVec2{
                FxScalar::from_int(copy_x) * stage_width,
                FxScalar::from_int(copy_y) * stage_height,
            });
        }
    }
    return offsets;
}

bool PointInAabb(const IVec2& point, FxAABB aabb) {
    const FxVec2 sim_point = FxVec2::from_int(point.x, point.y);
    return sim_point.x >= aabb.tl.x &&
           sim_point.x <= aabb.br.x &&
           sim_point.y >= aabb.tl.y &&
           sim_point.y <= aabb.br.y;
}

} // namespace

FVec2 GetNearestWorldDelta(const Stage& stage, const FVec2& from, const FVec2& to) {
    return FVec2::New(
        GetNearestWrappedDelta(from.x, to.x, static_cast<float>(stage.GetWidth()), stage.WrapsX()),
        GetNearestWrappedDelta(from.y, to.y, static_cast<float>(stage.GetHeight()), stage.WrapsY())
    );
}

FxVec2 GetNearestWorldDelta(const Stage& stage, FxVec2 from, FxVec2 to) {
    return FxVec2{
        GetNearestWrappedDelta(from.x,
                               to.x,
                               FxScalar::from_int(static_cast<std::int32_t>(stage.GetWidth())),
                               stage.WrapsX()),
        GetNearestWrappedDelta(from.y,
                               to.y,
                               FxScalar::from_int(static_cast<std::int32_t>(stage.GetHeight())),
                               stage.WrapsY()),
    };
}

FVec2 GetNearestWorldPoint(const Stage& stage, const FVec2& anchor, const FVec2& point) {
    return anchor + GetNearestWorldDelta(stage, anchor, point);
}

FxVec2 GetNearestWorldPoint(const Stage& stage, FxVec2 anchor, FxVec2 point) {
    return anchor + GetNearestWorldDelta(stage, anchor, point);
}

TileRotation GetTileRotationForQuery(const Stage& stage, const WorldTileQueryResult& tile_query) {
    if (tile_query.tile == nullptr ||
        !stage.IsTileCoordInside(tile_query.tile_pos.x, tile_query.tile_pos.y)) {
        return kTileRotation0;
    }
    return stage.GetTileRotation(
        static_cast<unsigned int>(tile_query.tile_pos.x),
        static_cast<unsigned int>(tile_query.tile_pos.y)
    );
}

bool IsTileQueryClimbable(const Stage& stage, const WorldTileQueryResult& tile_query) {
    if (tile_query.tile == nullptr) {
        return false;
    }
    return IsTileClimbableWithRotation(
        *tile_query.tile,
        GetTileRotationForQuery(stage, tile_query)
    );
}

FxAABB GetNearestWorldAabb(const Stage& stage, FxVec2 anchor, FxAABB aabb) {
    const FxVec2 center = aabb.center();
    const FxVec2 nearest_center = GetNearestWorldPoint(stage, anchor, center);
    return ShiftAabb(aabb, nearest_center - center);
}

bool WorldAabbContainsPoint(const Stage& stage, FxAABB area, FxVec2 point) {
    const FxAABB nearest_area = GetNearestWorldAabb(stage, point, area);
    return point.x >= nearest_area.tl.x && point.x <= nearest_area.br.x &&
           point.y >= nearest_area.tl.y && point.y <= nearest_area.br.y;
}

bool WorldAabbsIntersect(const Stage& stage, FxAABB area, FxAABB other) {
    const FxVec2 anchor = area.center();
    const FxAABB nearest_other = GetNearestWorldAabb(stage, anchor, other);
    return gfxp::aabbs_intersect(area, nearest_other);
}

std::vector<IVec2> GetTileCoordsInRect(const Stage& stage, const IVec2& tl, const IVec2& br) {
    std::vector<IVec2> result;
    const std::uint32_t tile_width = stage.GetTileWidth();
    const std::uint32_t tile_height = stage.GetTileHeight();
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
            FloorDiv(tl.x, static_cast<int>(kTileSize)),
            FloorDiv(tl.y, static_cast<int>(kTileSize))
        ),
        IVec2::New(
            FloorDiv(br.x, static_cast<int>(kTileSize)),
            FloorDiv(br.y, static_cast<int>(kTileSize))
        )
    );
}

std::vector<WorldTileQueryResult> QueryTilesInAabb(const Stage& stage, FxAABB area) {
    return QueryTilesInWorldRect(stage,
                                 IVec2::New(area.tl.x.floor_int(),
                                            area.tl.y.floor_int()),
                                 IVec2::New(area.br.x.floor_int(),
                                            area.br.y.floor_int()));
}

bool IsOneWayTopTileSupportingAabb(
    const Stage& stage,
    const WorldTileQueryResult& tile_query,
    FxAABB area
) {
    if (tile_query.tile == nullptr || !IsTileOneWayTopSolid(*tile_query.tile)) {
        return false;
    }

    const FxVec2 tile_tl =
        FxVec2::from_int(tile_query.tile_pos.x * static_cast<int>(kTileSize),
                               tile_query.tile_pos.y * static_cast<int>(kTileSize));
    const FxAABB tile_aabb = GetNearestWorldAabb(
        stage,
        area.center(),
        FxAABB::from_corners(
            tile_tl,
            tile_tl + FxVec2::from_int(static_cast<int>(kTileSize - 1),
                                             static_cast<int>(kTileSize - 1))
        )
    );
    return area.tl.y < tile_aabb.tl.y;
}

bool AabbTouchesBlockingStageBounds(const Stage& stage, FxAABB area) {
    if (area.tl.x < FxScalar::zero() &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Left)) {
        return true;
    }
    if (area.tl.y < FxScalar::zero() &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Top)) {
        return true;
    }
    if (area.br.x > FxScalar::from_int(static_cast<std::int32_t>(stage.GetWidth() - 1)) &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Right)) {
        return true;
    }
    if (area.br.y > FxScalar::from_int(static_cast<std::int32_t>(stage.GetHeight() - 1)) &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Bottom)) {
        return true;
    }
    return false;
}

bool AabbHitsBlockingTiles(const Stage& stage, FxAABB area) {
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(stage, area)) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return true;
        }
    }
    return false;
}

bool AabbHitsBlockingWorldGeometry(const Stage& stage, FxAABB area) {
    return AabbTouchesBlockingStageBounds(stage, area) || AabbHitsBlockingTiles(stage, area);
}

bool AabbHitsImpassableEnts(
    const State& state,
    const Graphics& graphics,
    FxAABB area,
    std::optional<VID> exclude_vid
) {
    const FxVec2 anchor = area.center();
    for (const VID& vid : QueryEntsInAabb(state, area, exclude_vid)) {
        const Ent* const ent = state.ents.GetEnt(vid);
        if (ent == nullptr || !ent->active || !ent->impassable) {
            continue;
        }

        const FxAABB ent_aabb = GetNearestWorldAabb(
            state.stage,
            anchor,
            ents::common::GetContactAabbForEnt(*ent, graphics)
        );
        if (gfxp::aabbs_intersect(area, ent_aabb)) {
            return true;
        }
    }
    return false;
}

bool AabbHitsBlockingWorldGeometryOrImpassableEnts(
    const State& state,
    const Graphics& graphics,
    FxAABB area,
    std::optional<VID> exclude_vid
) {
    return AabbHitsBlockingWorldGeometry(state.stage, area) ||
           AabbHitsImpassableEnts(state, graphics, area, exclude_vid);
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

std::optional<WorldTileQueryResult> QueryTileAtWorldPos(const Stage& stage, FxVec2 world_pos) {
    return QueryTileAtWorldPos(
        stage,
        IVec2::New(world_pos.x.trunc_int(), world_pos.y.trunc_int())
    );
}

std::vector<VID> QueryEntsInAabb(
    const State& state,
    FxAABB area,
    std::optional<VID> exclude_vid
) {
    std::vector<VID> result;
    std::vector<bool> seen(state.ents.ents.size(), false);
    const std::vector<FxVec2> offsets = GetQueryOffsets(state.stage, area);

    for (const FxVec2& offset : offsets) {
        FxAABB sample_area = area;
        sample_area.translate(-offset);
        const std::vector<VID> hits = state.sid.Query(sample_area);
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

    std::sort(result.begin(), result.end(), VidLess);
    return result;
}

struct RaycastTarget {
    VID vid;
    FxAABB aabb;
};

std::vector<RaycastTarget> CollectRaycastTargets(
    const Ent& source_ent,
    FxVec2 start_pos,
    FxAABB ray_aabb,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
) {
    const std::vector<VID> hits = QueryEntsInAabb(state, ray_aabb, source_ent.vid);

    std::vector<RaycastTarget> targets;
    targets.reserve(hits.size());
    for (const VID& vid : hits) {
        if (owner_vid.has_value() && vid == *owner_vid) {
            continue;
        }

        const Ent* const ent = state.ents.GetEnt(vid);
        if (ent == nullptr || !ent->active) {
            continue;
        }
        if (!ent->can_be_hit) {
            continue;
        }
        if (owner_vid.has_value() && ent->held_by_vid.has_value() &&
            ent->held_by_vid == owner_vid) {
            continue;
        }

        targets.push_back(RaycastTarget{
            .vid = vid,
            .aabb = GetNearestWorldAabb(
                state.stage,
                start_pos,
                ents::common::GetContactAabbForEnt(*ent, graphics)
            ),
        });
    }

    return targets;
}

std::optional<WorldRayHit> QueryEntRayHitAtPoint(
    const IVec2& point,
    const std::vector<RaycastTarget>& targets
) {
    for (const RaycastTarget& target : targets) {
        if (!PointInAabb(point, target.aabb)) {
            continue;
        }
        return WorldRayHit{
            .type = WorldRayHitType::Ent,
            .point = point,
            .ent_vid = target.vid,
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

    if (const std::optional<WorldRayHit> ent_hit = QueryEntRayHitAtPoint(point, targets)) {
        return *ent_hit;
    }

    return WorldRayHit{};
}

TileStepRaycastResult RaycastTileSteps(
    const Stage& stage,
    const IVec2& origin_tile,
    const IVec2& direction,
    int max_steps
) {
    TileStepRaycastResult result;
    result.last_open_tile = origin_tile;
    result.last_open_unwrapped_tile = origin_tile;
    result.blocker_unwrapped_tile = origin_tile;

    if (max_steps <= 0 || (direction.x == 0 && direction.y == 0) ||
        !stage.IsTileCoordInside(origin_tile.x, origin_tile.y)) {
        return result;
    }

    const Tile origin = stage.GetTile(
        static_cast<unsigned int>(origin_tile.x),
        static_cast<unsigned int>(origin_tile.y)
    );
    if (IsTileCollidable(origin)) {
        return result;
    }

    for (int step = 1; step <= max_steps; ++step) {
        const IVec2 sample_unwrapped = IVec2::New(
            origin_tile.x + direction.x * step,
            origin_tile.y + direction.y * step
        );
        const Tile sample_tile = stage.GetTileOrBorder(sample_unwrapped.x, sample_unwrapped.y);
        if (IsTileCollidable(sample_tile)) {
            result.blocked = true;
            result.blocker_unwrapped_tile = sample_unwrapped;
            const IVec2 blocker_tile = stage.WrapTileCoord(sample_unwrapped);
            if (stage.IsTileCoordInside(blocker_tile.x, blocker_tile.y)) {
                result.blocker_tile = blocker_tile;
            }
            break;
        }

        const IVec2 wrapped_sample = stage.WrapTileCoord(sample_unwrapped);
        if (stage.IsTileCoordInside(wrapped_sample.x, wrapped_sample.y)) {
            result.last_open_tile = wrapped_sample;
        }
        result.last_open_unwrapped_tile = sample_unwrapped;
        result.open_steps = step;
    }

    return result;
}

WorldRayHit RaycastRenderTiles(
    const FVec2& start_pos,
    const FVec2& direction,
    int max_distance,
    const State& state
) {
    const FVec2 step_dir = NormalizeOrZeroDeterministic(direction);
    if (max_distance <= 0 || step_dir == FVec2::New(0.0F, 0.0F)) {
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
    const Ent& source_ent,
    FxVec2 start_pos,
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
    const int start_x = start_pos.x.trunc_int();
    const int ray_y = start_pos.y.trunc_int();
    const int end_x = start_x + (step_dir * max_distance);
    const FxAABB ray_aabb = FxAABB::from_corners(
        FxVec2::from_int(std::min(start_x, end_x), ray_y),
        FxVec2::from_int(std::max(start_x, end_x), ray_y)
    );
    const std::vector<RaycastTarget> targets =
        CollectRaycastTargets(source_ent, start_pos, ray_aabb, state, graphics, owner_vid);

    for (int step = 0; step < max_distance; ++step) {
        const IVec2 point = IVec2::New(start_x + (step_dir * step), ray_y);
        const WorldRayHit hit = QueryWorldRayHitAtPoint(point, state, targets);
        if (hit.type != WorldRayHitType::None) {
            return hit;
        }
    }

    return WorldRayHit{};
}

} // namespace splonks

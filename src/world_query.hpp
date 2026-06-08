#pragma once

#include "ent.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks {

Vec2 GetNearestWorldDelta(const Stage& stage, const Vec2& from, const Vec2& to);
Vec2 GetNearestWorldPoint(const Stage& stage, const Vec2& anchor, const Vec2& point);
AABB GetNearestWorldAabb(const Stage& stage, const Vec2& anchor, const AABB& aabb);
sim::Vec2 GetNearestWorldDelta(const Stage& stage, sim::Vec2 from, sim::Vec2 to);
sim::Vec2 GetNearestWorldPoint(const Stage& stage, sim::Vec2 anchor, sim::Vec2 point);
sim::AABB GetNearestWorldAabb(const Stage& stage, sim::Vec2 anchor, sim::AABB aabb);
bool WorldAabbContainsPoint(const Stage& stage, const AABB& area, const Vec2& point);
bool WorldAabbsIntersect(const Stage& stage, const AABB& area, const AABB& other);
bool WorldAabbContainsPoint(const Stage& stage, sim::AABB area, sim::Vec2 point);
bool WorldAabbsIntersect(const Stage& stage, sim::AABB area, sim::AABB other);
std::vector<IVec2> GetTileCoordsInRect(const Stage& stage, const IVec2& tl, const IVec2& br);

struct WorldTileQueryResult {
    IVec2 tile_pos = IVec2::New(0, 0);
    const Tile* tile = nullptr;
};

TileRotation GetTileRotationForQuery(const Stage& stage, const WorldTileQueryResult& tile_query);
bool IsTileQueryClimbable(const Stage& stage, const WorldTileQueryResult& tile_query);

std::vector<WorldTileQueryResult> QueryTilesInRect(
    const Stage& stage,
    const IVec2& tl,
    const IVec2& br
);
std::vector<WorldTileQueryResult> QueryTilesInWorldRect(
    const Stage& stage,
    const IVec2& tl,
    const IVec2& br
);
std::vector<WorldTileQueryResult> QueryTilesInAabb(const Stage& stage, const AABB& area);
std::vector<WorldTileQueryResult> QueryTilesInAabb(const Stage& stage, sim::AABB area);
bool IsOneWayTopTileSupportingAabb(
    const Stage& stage,
    const WorldTileQueryResult& tile_query,
    const AABB& area
);
bool IsOneWayTopTileSupportingAabb(
    const Stage& stage,
    const WorldTileQueryResult& tile_query,
    sim::AABB area
);
bool AabbTouchesBlockingStageBounds(const Stage& stage, sim::AABB area);
bool AabbHitsBlockingTiles(const Stage& stage, sim::AABB area);
bool AabbHitsBlockingWorldGeometry(const Stage& stage, sim::AABB area);
bool AabbHitsImpassableEnts(
    const State& state,
    const Graphics& graphics,
    sim::AABB area,
    std::optional<VID> exclude_vid = std::nullopt
);
bool AabbHitsBlockingWorldGeometryOrImpassableEnts(
    const State& state,
    const Graphics& graphics,
    sim::AABB area,
    std::optional<VID> exclude_vid = std::nullopt
);
std::optional<WorldTileQueryResult> QueryTileAtTilePos(const Stage& stage, const IVec2& tile_pos);
std::optional<WorldTileQueryResult> QueryTileAtWorldPos(const Stage& stage, const IVec2& world_pos);
std::optional<WorldTileQueryResult> QueryTileAtWorldPos(const Stage& stage, sim::Vec2 world_pos);
std::vector<VID> QueryEntsInAabb(
    const State& state,
    const AABB& area,
    std::optional<VID> exclude_vid = std::nullopt
);
std::vector<VID> QueryEntsInAabb(
    const State& state,
    sim::AABB area,
    std::optional<VID> exclude_vid = std::nullopt
);

enum class WorldRayHitType {
    None,
    StageBounds,
    Tile,
    Ent,
};

struct WorldRayHit {
    WorldRayHitType type = WorldRayHitType::None;
    IVec2 point = IVec2::New(0, 0);
    std::optional<IVec2> tile_pos = std::nullopt;
    std::optional<VID> ent_vid = std::nullopt;
};

struct TileStepRaycastResult {
    IVec2 last_open_tile = IVec2::New(0, 0);
    IVec2 last_open_unwrapped_tile = IVec2::New(0, 0);
    std::optional<IVec2> blocker_tile = std::nullopt;
    IVec2 blocker_unwrapped_tile = IVec2::New(0, 0);
    bool blocked = false;
    int open_steps = 0;
};

TileStepRaycastResult RaycastTileSteps(
    const Stage& stage,
    const IVec2& origin_tile,
    const IVec2& direction,
    int max_steps
);

WorldRayHit RaycastTiles(
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state
);

WorldRayHit RaycastHorizontal(
    const Ent& source_ent,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

WorldRayHit RaycastVertical(
    const Ent& source_ent,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

WorldRayHit RaycastEnts(
    const Ent& source_ent,
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

WorldRayHit RaycastWorld(
    const Ent& source_ent,
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

} // namespace splonks

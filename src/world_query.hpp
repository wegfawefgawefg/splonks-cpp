#pragma once

#include "ent.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks {

FVec2 GetNearestWorldDelta(const Stage& stage, const FVec2& from, const FVec2& to);
FVec2 GetNearestWorldPoint(const Stage& stage, const FVec2& anchor, const FVec2& point);
sim::FxVec2 GetNearestWorldDelta(const Stage& stage, sim::FxVec2 from, sim::FxVec2 to);
sim::FxVec2 GetNearestWorldPoint(const Stage& stage, sim::FxVec2 anchor, sim::FxVec2 point);
sim::AABB GetNearestWorldAabb(const Stage& stage, sim::FxVec2 anchor, sim::AABB aabb);
bool WorldAabbContainsPoint(const Stage& stage, sim::AABB area, sim::FxVec2 point);
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
std::vector<WorldTileQueryResult> QueryTilesInAabb(const Stage& stage, sim::AABB area);
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
std::optional<WorldTileQueryResult> QueryTileAtWorldPos(const Stage& stage, sim::FxVec2 world_pos);
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

WorldRayHit RaycastRenderTiles(
    const FVec2& start_pos,
    const FVec2& direction,
    int max_distance,
    const State& state
);

WorldRayHit RaycastHorizontal(
    const Ent& source_ent,
    sim::FxVec2 start_pos,
    int direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

} // namespace splonks

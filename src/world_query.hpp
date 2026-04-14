#pragma once

#include "entity.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks {

Vec2 GetNearestWorldDelta(const Stage& stage, const Vec2& from, const Vec2& to);
Vec2 GetNearestWorldPoint(const Stage& stage, const Vec2& anchor, const Vec2& point);
AABB GetNearestWorldAabb(const Stage& stage, const Vec2& anchor, const AABB& aabb);
std::vector<IVec2> GetTileCoordsInRect(const Stage& stage, const IVec2& tl, const IVec2& br);

struct WorldTileQueryResult {
    IVec2 tile_pos = IVec2::New(0, 0);
    const Tile* tile = nullptr;
};

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
bool AabbTouchesBlockingStageBounds(const Stage& stage, const AABB& area);
bool AabbHitsBlockingTiles(const Stage& stage, const AABB& area);
bool AabbHitsBlockingWorldGeometry(const Stage& stage, const AABB& area);
bool AabbHitsImpassableEntities(
    const State& state,
    const Graphics& graphics,
    const AABB& area,
    std::optional<VID> exclude_vid = std::nullopt
);
bool AabbHitsBlockingWorldGeometryOrImpassableEntities(
    const State& state,
    const Graphics& graphics,
    const AABB& area,
    std::optional<VID> exclude_vid = std::nullopt
);
std::optional<WorldTileQueryResult> QueryTileAtTilePos(const Stage& stage, const IVec2& tile_pos);
std::optional<WorldTileQueryResult> QueryTileAtWorldPos(const Stage& stage, const IVec2& world_pos);
std::vector<VID> QueryEntitiesInAabb(
    const State& state,
    const AABB& area,
    std::optional<VID> exclude_vid = std::nullopt
);

enum class WorldRayHitType {
    None,
    StageBounds,
    Tile,
    Entity,
};

struct WorldRayHit {
    WorldRayHitType type = WorldRayHitType::None;
    IVec2 point = IVec2::New(0, 0);
    std::optional<IVec2> tile_pos = std::nullopt;
    std::optional<VID> entity_vid = std::nullopt;
};

WorldRayHit RaycastTiles(
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state
);

WorldRayHit RaycastHorizontal(
    const Entity& source_entity,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

WorldRayHit RaycastVertical(
    const Entity& source_entity,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

WorldRayHit RaycastEntities(
    const Entity& source_entity,
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

WorldRayHit RaycastWorld(
    const Entity& source_entity,
    const Vec2& start_pos,
    const Vec2& direction,
    int max_distance,
    const State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid = std::nullopt
);

} // namespace splonks

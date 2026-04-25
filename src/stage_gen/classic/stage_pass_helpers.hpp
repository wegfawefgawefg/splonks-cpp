#pragma once

#include "entity/core_types.hpp"
#include "quest.hpp"
#include "stage.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace splonks::stage_gen::classic {

int PickStagePassIndex(std::size_t size);
std::string GetPassString(const StagePassConfig& pass, std::string_view key,
                          std::string_view fallback = "");
bool TileCoordExists(const Stage& stage, int tile_x, int tile_y);
bool IsCollidableTileAt(const Stage& stage, int tile_x, int tile_y);
IVec2 GetRoomAtTileCoord(const Stage& stage, int tile_x, int tile_y);
bool IsShopRoomAt(const Stage& stage, int tile_x, int tile_y);
bool IsStartRoomAt(const Stage& stage, int tile_x, int tile_y);
bool HasSpawnAtWorldPos(const Stage& stage, const Vec2& pos);
std::optional<Vec2> FindEntrancePos(const Stage& stage);
std::optional<Vec2> FindExitPos(const Stage& stage);
bool HasSpawnType(const Stage& stage, EntityType type_);
float DistanceToNearestSpawnType(const Stage& stage, EntityType type_, const Vec2& pos);
bool HasExitSpawn(const Stage& stage, std::string_view exit_id);
void AddAmbientSpawn(Stage& stage, EntityType type_, const Vec2& pos,
                     LeftOrRight facing = LeftOrRight::Left);
bool IsTreasureSpawnType(EntityType type_);

} // namespace splonks::stage_gen::classic

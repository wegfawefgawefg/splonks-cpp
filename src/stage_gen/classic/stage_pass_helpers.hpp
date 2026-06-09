#pragma once

#include "ent/core_types.hpp"
#include "quest.hpp"
#include "stage.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace splonks::stage_gen::classic {

int PickStagePassIndex(std::size_t size, DetRng& det_rng);
std::string GetPassString(const StagePassConfig& pass, std::string_view key,
                          std::string_view fallback = "");
bool TileCoordExists(const Stage& stage, int tile_x, int tile_y);
bool IsCollidableTileAt(const Stage& stage, int tile_x, int tile_y);
IVec2 GetRoomAtTileCoord(const Stage& stage, int tile_x, int tile_y);
bool IsShopRoomAt(const Stage& stage, int tile_x, int tile_y);
bool IsStartRoomAt(const Stage& stage, int tile_x, int tile_y);
bool HasSpawnAtWorldPos(const Stage& stage, const FVec2& pos);
std::optional<FVec2> FindEntrancePos(const Stage& stage);
std::optional<FVec2> FindExitPos(const Stage& stage);
bool HasSpawnType(const Stage& stage, EntType type_);
float DistanceSqToNearestSpawnType(const Stage& stage, EntType type_, const FVec2& pos);
bool HasExitSpawn(const Stage& stage, std::string_view exit_id);
void AddAmbientSpawn(Stage& stage, EntType type_, const FVec2& pos,
                     Side facing = Side::Left);
bool IsTreasureSpawnType(EntType type_);

} // namespace splonks::stage_gen::classic

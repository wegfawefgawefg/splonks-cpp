#pragma once

#include "quest.hpp"
#include "stage.hpp"
#include "stage_gen/classic/item_pools.hpp"
#include "stage_gen/classic/room_templates.hpp"

#include <string>
#include <vector>

namespace splonks::stage_gen::classic {

struct ResolvedRoom {
    std::vector<std::vector<Tile>> tiles;
    std::vector<EntSpawn> ent_spawns;
    std::vector<StageTileTrigger> tile_triggers;
    std::vector<BackgroundStamp> background_stamps;
    std::string source_path;
};

ResolvedRoom ResolveRoom(int room_code, int level_number, bool is_start_room, bool is_end_room, int room_code_above,
                         bool jungle_lake_active, const UVec2& room_size,
                         const Stage& existing_stage, const ClassicRoomTemplateDb& room_templates,
                         const GlyphMap& glyph_map, const ItemPoolDb& item_db,
                         const ShopConfigDb& shop_db);

} // namespace splonks::stage_gen::classic

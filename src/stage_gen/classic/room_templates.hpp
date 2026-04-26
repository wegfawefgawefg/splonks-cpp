#pragma once

#include "quest.hpp"
#include "stage_gen/room_template_loader.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace splonks::stage_gen::classic {

enum class ShopType {
    None,
    General,
    Bomb,
    Weapon,
    Rare,
    Clothing,
    Craps,
    Kissing,
};

struct RoomTemplateSelection {
    std::string glyphs;
    std::string source_path;
    ShopType shop_type = ShopType::None;
};

struct ClassicRoomTemplateDb {
    std::string stage_id;
    std::vector<::splonks::stage_gen::RoomTemplate> start;
    std::vector<::splonks::stage_gen::RoomTemplate> exit;
    std::vector<::splonks::stage_gen::RoomTemplate> side;
    std::vector<::splonks::stage_gen::RoomTemplate> main;
    std::vector<::splonks::stage_gen::RoomTemplate> exit_main;
    std::vector<::splonks::stage_gen::RoomTemplate> shop_left;
    std::vector<::splonks::stage_gen::RoomTemplate> shop_right;
    std::vector<::splonks::stage_gen::RoomTemplate> special_6;
    std::vector<::splonks::stage_gen::RoomTemplate> special_7;
    std::vector<::splonks::stage_gen::RoomTemplate> special_8;
    std::vector<::splonks::stage_gen::RoomTemplate> special_9;
    std::vector<::splonks::stage_gen::RoomTemplate> snake_pit_top;
    std::vector<::splonks::stage_gen::RoomTemplate> snake_pit_bottom;
    std::vector<::splonks::stage_gen::RoomTemplate> vault;
    std::vector<::splonks::stage_gen::RoomTemplate> idol;
    std::vector<::splonks::stage_gen::RoomTemplate> altar;
    std::vector<::splonks::stage_gen::RoomTemplate> drop;
};

ClassicRoomTemplateDb LoadClassicRoomTemplateDb(const StageConfig& config);
RoomTemplateSelection SelectRoomTemplate(int room_code, bool is_start_room, bool is_end_room,
                                         int room_code_above, bool jungle_lake_active,
                                         const ClassicRoomTemplateDb& room_templates);
std::string ExpandObstacles(std::string_view template_glyphs);
std::string_view ShopTypeId(ShopType shop_type);

} // namespace splonks::stage_gen::classic

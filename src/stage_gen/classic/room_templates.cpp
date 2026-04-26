#include "stage_gen/classic/room_templates.hpp"

#include "stage_gen/classic/room_layout.hpp"
#include "utils.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace splonks::stage_gen::classic {

using RoomTemplate = ::splonks::stage_gen::RoomTemplate;
using GlyphPatch = std::string_view;

namespace {

int PickIndex(std::size_t size) {
    return rng::RandomIntInclusive(0, static_cast<int>(size) - 1);
}

ShopType ParseShopType(std::string_view value) {
    if (value == "general") {
        return ShopType::General;
    }
    if (value == "bomb") {
        return ShopType::Bomb;
    }
    if (value == "weapon") {
        return ShopType::Weapon;
    }
    if (value == "rare") {
        return ShopType::Rare;
    }
    if (value == "clothing") {
        return ShopType::Clothing;
    }
    if (value == "craps") {
        return ShopType::Craps;
    }
    if (value == "kissing") {
        return ShopType::Kissing;
    }
    return ShopType::None;
}

std::string GetRoomPoolPath(const StageConfig& config, std::string_view pool_name) {
    const auto it = config.room_pools.find(std::string(pool_name));
    if (it == config.room_pools.end()) {
        return "";
    }
    return std::string(GetClassicQuestRootPath()) + "/" + it->second;
}

std::vector<RoomTemplate> LoadClassicRoomPool(const StageConfig& config, std::string_view pool_name) {
    const std::string path = GetRoomPoolPath(config, pool_name);
    if (path.empty()) {
        return {};
    }

    std::vector<RoomTemplate> rooms = ::splonks::stage_gen::LoadRoomTemplatePool(path);
    if (rooms.empty()) {
        throw std::runtime_error("Classic room pool is configured but empty or missing: " + path);
    }
    return rooms;
}

const RoomTemplate* PickLoadedTemplate(const std::vector<RoomTemplate>& templates,
                                       std::size_t first_index, std::size_t last_index_exclusive) {
    if (first_index >= templates.size()) {
        return nullptr;
    }
    const std::size_t last_index = std::min(last_index_exclusive, templates.size());
    if (last_index <= first_index) {
        return nullptr;
    }

    int total_weight = 0;
    for (std::size_t i = first_index; i < last_index; ++i) {
        total_weight += std::max(templates[i].weight, 0);
    }
    if (total_weight <= 0) {
        return &templates[static_cast<std::size_t>(rng::RandomIntInclusive(
            static_cast<int>(first_index), static_cast<int>(last_index - 1)))];
    }

    int roll = rng::RandomIntInclusive(1, total_weight);
    for (std::size_t i = first_index; i < last_index; ++i) {
        roll -= std::max(templates[i].weight, 0);
        if (roll <= 0) {
            return &templates[i];
        }
    }

    return &templates[last_index - 1];
}

const RoomTemplate* PickLoadedTemplate(const std::vector<RoomTemplate>& templates) {
    return PickLoadedTemplate(templates, 0, templates.size());
}

RoomTemplateSelection MakeLoadedRoomSelection(const RoomTemplate& room) {
    RoomTemplateSelection selection;
    selection.glyphs = room.grid;
    selection.source_path = room.source_path;
    if (const auto it = room.properties.find("shop_type"); it != room.properties.end()) {
        selection.shop_type = ParseShopType(it->second);
    }
    return selection;
}

RoomTemplateSelection RequireRoomSelection(const RoomTemplate* room, std::string_view pool_name) {
    if (room == nullptr) {
        throw std::runtime_error("Classic room pool required but empty: " + std::string(pool_name));
    }
    return MakeLoadedRoomSelection(*room);
}

void RequirePoolSize(std::string_view stage_id, std::string_view pool_name, std::size_t actual,
                     std::size_t expected) {
    if (actual == expected) {
        return;
    }
    throw std::runtime_error("Classic " + std::string(stage_id) + " " + std::string(pool_name) +
                             " pool expected " + std::to_string(expected) + " templates, found " +
                             std::to_string(actual));
}

bool IsTempleLikeStage(std::string_view stage_id) {
    return stage_id == "temple" || stage_id == "city_of_gold" || stage_id == "haunted_castle";
}

const RoomTemplate* PickStartRoomTemplate(const ClassicRoomTemplateDb& room_templates,
                                          bool starts_with_drop) {
    const std::size_t count = room_templates.start.size();
    if (count == 0) {
        return nullptr;
    }

    if (room_templates.stage_id == "mines") {
        RequirePoolSize(room_templates.stage_id, "start", count, 8);
        return starts_with_drop ? PickLoadedTemplate(room_templates.start, 4, 8)
                                : PickLoadedTemplate(room_templates.start, 0, 4);
    }
    if (room_templates.stage_id == "jungle" || room_templates.stage_id == "black_market") {
        RequirePoolSize(room_templates.stage_id, "start", count, 4);
        return starts_with_drop ? PickLoadedTemplate(room_templates.start, 2, 4)
                                : PickLoadedTemplate(room_templates.start, 0, 2);
    }
    if (room_templates.stage_id == "ice_caves" || IsTempleLikeStage(room_templates.stage_id)) {
        RequirePoolSize(room_templates.stage_id, "start", count, 2);
        return starts_with_drop ? PickLoadedTemplate(room_templates.start, 1, 2)
                                : PickLoadedTemplate(room_templates.start, 0, 1);
    }
    if (room_templates.stage_id == "olmec_lair") {
        RequirePoolSize(room_templates.stage_id, "start", count, 6);
        return PickLoadedTemplate(room_templates.start);
    }

    throw std::runtime_error("Classic room selector missing start rules for stage: " +
                             room_templates.stage_id);
}

const RoomTemplate* PickExitRoomTemplate(const ClassicRoomTemplateDb& room_templates,
                                         bool room_above_is_drop, bool jungle_lake_active) {
    const std::size_t count = room_templates.exit.size();
    if (count == 0) {
        return nullptr;
    }

    if (room_templates.stage_id == "mines") {
        RequirePoolSize(room_templates.stage_id, "exit", count, 6);
        return room_above_is_drop ? PickLoadedTemplate(room_templates.exit, 1, 4)
                                  : PickLoadedTemplate(room_templates.exit, 2, 6);
    }
    if (room_templates.stage_id == "jungle") {
        RequirePoolSize(room_templates.stage_id, "exit", count, 5);
        if (jungle_lake_active) {
            return PickLoadedTemplate(room_templates.exit, 4, 5);
        }
        return room_above_is_drop ? PickLoadedTemplate(room_templates.exit, 0, 2)
                                  : PickLoadedTemplate(room_templates.exit, 2, 4);
    }
    if (room_templates.stage_id == "black_market") {
        RequirePoolSize(room_templates.stage_id, "exit", count, 4);
        return room_above_is_drop ? PickLoadedTemplate(room_templates.exit, 0, 2)
                                  : PickLoadedTemplate(room_templates.exit, 2, 4);
    }
    if (room_templates.stage_id == "ice_caves" || IsTempleLikeStage(room_templates.stage_id)) {
        RequirePoolSize(room_templates.stage_id, "exit", count, 1);
        return PickLoadedTemplate(room_templates.exit);
    }
    if (room_templates.stage_id == "olmec_lair") {
        RequirePoolSize(room_templates.stage_id, "exit", count, 6);
        return PickLoadedTemplate(room_templates.exit);
    }

    throw std::runtime_error("Classic room selector missing exit rules for stage: " +
                             room_templates.stage_id);
}

const std::array<GlyphPatch, 8> kDoorwayObstaclePatches = {
    "009002111221112", "009000212002120", "000000000092222", "000000000022229",
    "000001100119001", "000001001110091", "111111000140094", "000001202112921",
};

const std::array<GlyphPatch, 16> kGroundObstaclePatches = {
    "111110000000000", "000000111100000", "000000111100000", "000000000011111",
    "000002020017177", "000000202071717", "000000020277171", "000002220011100",
    "000000222001110", "000000022200111", "111002220000000", "011100222000000",
    "001110022200000", "000000222021112", "000002010077117", "000000010271177",
};

const std::array<GlyphPatch, 10> kAirObstaclePatches = {
    "111110000000000", "222220000000000", "111002220000000", "011100222000000",
    "001110022200000", "000000111000000", "000000111002220", "000000222001110",
    "000000022001111", "000002220011100",
};

GlyphPatch PickObstaclePatch(char glyph) {
    switch (glyph) {
    case '8':
        return kDoorwayObstaclePatches[static_cast<std::size_t>(
            PickIndex(kDoorwayObstaclePatches.size()))];
    case '5':
        return kGroundObstaclePatches[static_cast<std::size_t>(
            PickIndex(kGroundObstaclePatches.size()))];
    case '6':
        return kAirObstaclePatches[static_cast<std::size_t>(PickIndex(kAirObstaclePatches.size()))];
    default:
        return "";
    }
}

void ApplyPatchAt(std::string& glyphs, int top_left_index, GlyphPatch patch) {
    const int start_x = top_left_index % 10;
    const int start_y = top_left_index / 10;

    for (int patch_y = 0; patch_y < 3; ++patch_y) {
        for (int patch_x = 0; patch_x < 5; ++patch_x) {
            const int x = start_x + patch_x;
            const int y = start_y + patch_y;
            if (x >= 10 || y >= 8) {
                continue;
            }
            glyphs[static_cast<std::size_t>(y * 10 + x)] =
                patch[static_cast<std::size_t>(patch_y * 5 + patch_x)];
        }
    }
}

} // namespace

ClassicRoomTemplateDb LoadClassicRoomTemplateDb(const StageConfig& config) {
    ClassicRoomTemplateDb db;
    db.stage_id = config.id;
    db.start = LoadClassicRoomPool(config, "start");
    db.exit = LoadClassicRoomPool(config, "exit");
    db.side = LoadClassicRoomPool(config, "side");
    db.main = LoadClassicRoomPool(config, "main");
    db.exit_main = LoadClassicRoomPool(config, "exit_main");
    db.shop_left = LoadClassicRoomPool(config, "shop_left");
    db.shop_right = LoadClassicRoomPool(config, "shop_right");
    db.special_6 = LoadClassicRoomPool(config, "special_6");
    db.special_7 = LoadClassicRoomPool(config, "special_7");
    db.special_8 = LoadClassicRoomPool(config, "special_8");
    db.special_9 = LoadClassicRoomPool(config, "special_9");
    db.snake_pit_top = LoadClassicRoomPool(config, "snake_pit_top");
    db.snake_pit_bottom = LoadClassicRoomPool(config, "snake_pit_bottom");
    db.vault = LoadClassicRoomPool(config, "vault");
    db.idol = LoadClassicRoomPool(config, "idol");
    db.drop = LoadClassicRoomPool(config, "drop");
    return db;
}

std::string ExpandObstacles(std::string_view template_glyphs) {
    std::string glyphs(template_glyphs);
    for (int index = 0; index < 80; ++index) {
        const char glyph = glyphs[static_cast<std::size_t>(index)];
        if (glyph != '5' && glyph != '6' && glyph != '8') {
            continue;
        }
        ApplyPatchAt(glyphs, index, PickObstaclePatch(glyph));
    }
    return glyphs;
}

RoomTemplateSelection SelectRoomTemplate(int room_code, bool is_start_room, bool is_end_room,
                                         int room_code_above, bool jungle_lake_active,
                                         const ClassicRoomTemplateDb& room_templates) {
    if (is_start_room) {
        const bool starts_with_drop = room_code == static_cast<int>(RoomCode::Drop);
        return RequireRoomSelection(
            PickStartRoomTemplate(room_templates, starts_with_drop),
            starts_with_drop ? "start/drop" : "start"
        );
    }

    if (is_end_room) {
        return RequireRoomSelection(
            PickExitRoomTemplate(room_templates, room_code_above == static_cast<int>(RoomCode::Drop),
                                 jungle_lake_active),
            "exit"
        );
    }

    switch (room_code) {
    case static_cast<int>(RoomCode::Side):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.side), "side");
    case static_cast<int>(RoomCode::Main):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.main), "main");
    case static_cast<int>(RoomCode::Drop):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.drop), "drop");
    case static_cast<int>(RoomCode::Exit):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.exit_main), "exit_main");
    case static_cast<int>(RoomCode::ShopLeft):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.shop_left), "shop_left");
    case static_cast<int>(RoomCode::ShopRight):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.shop_right), "shop_right");
    case static_cast<int>(RoomCode::Special6):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.special_6), "special_6");
    case static_cast<int>(RoomCode::Special7):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.special_7), "special_7");
    case static_cast<int>(RoomCode::Special8):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.special_8), "special_8");
    case static_cast<int>(RoomCode::Special9):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.special_9), "special_9");
    case static_cast<int>(RoomCode::SnakePitTop):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.snake_pit_top),
                                    "snake_pit_top");
    case static_cast<int>(RoomCode::SnakePitBottom):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.snake_pit_bottom),
                                    "snake_pit_bottom");
    case static_cast<int>(RoomCode::Vault):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.vault), "vault");
    case static_cast<int>(RoomCode::Idol):
        return RequireRoomSelection(PickLoadedTemplate(room_templates.idol), "idol");
    default:
        throw std::runtime_error("Unknown classic room code: " + std::to_string(room_code));
    }
}

std::string_view ShopTypeId(ShopType shop_type) {
    switch (shop_type) {
    case ShopType::General:
        return "general";
    case ShopType::Bomb:
        return "bomb";
    case ShopType::Weapon:
        return "weapon";
    case ShopType::Rare:
        return "rare";
    case ShopType::Clothing:
        return "clothing";
    case ShopType::Craps:
        return "craps";
    case ShopType::Kissing:
        return "kissing";
    case ShopType::None:
        return "none";
    }
    return "none";
}

} // namespace splonks::stage_gen::classic

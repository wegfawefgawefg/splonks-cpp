#include "stage_gen/classic/glyph_actions.hpp"

#include "aframe_id.hpp"
#include "stage_gen/classic/room_layout.hpp"
#include "ents/shop_tile_triggers.hpp"
#include "stage_gen/classic/tile_palette.hpp"
#include "utils.hpp"

#include <stdexcept>
#include <vector>

namespace splonks::stage_gen::classic {

namespace {

Vec2 GetShrineIdolTopLeft(const Vec2& tile_pos) {
    // The shrine idol rests on the seam between the two altar base tiles below it.
    // Spawn it already settled so the tiki head does not false-trigger from the
    // idol's initial gravity settle on frame one.
    return tile_pos + Vec2::New(10.0F, 4.0F);
}

EntType GetShopSignEntType(ShopType shop_type, const ShopConfigDb& shop_db) {
    const ShopTypeConfig* config = shop_db.FindShopType(ShopTypeId(shop_type));
    return config == nullptr ? EntType::None : config->sign;
}

bool IsShopRoomCode(int room_code) {
    return room_code == static_cast<int>(RoomCode::ShopLeft) ||
           room_code == static_cast<int>(RoomCode::ShopRight);
}

bool IsShopAreaRoomCode(int room_code) {
    return IsShopRoomCode(room_code) || room_code == static_cast<int>(RoomCode::Vault);
}

std::uint32_t GetClassicShopBasePrice(EntType type_) {
    switch (type_) {
    case EntType::Bow:
        return 1000;
    case EntType::WebCannon:
        return 2000;
    case EntType::Parachute:
        return 2000;
    case EntType::BombBag:
        return 2500;
    case EntType::RopePile:
        return 2500;
    case EntType::Paste:
        return 3000;
    case EntType::Compass:
        return 3000;
    case EntType::SpikeShoes:
        return 4000;
    case EntType::Mitt:
        return 4000;
    case EntType::Pistol:
        return 5000;
    case EntType::SpringShoes:
        return 5000;
    case EntType::Machete:
        return 7000;
    case EntType::Gloves:
        return 8000;
    case EntType::Spectacles:
        return 8000;
    case EntType::Mattock:
        return 8000;
    case EntType::Teleporter:
    case EntType::TeleporterBackpack:
        return 10000;
    case EntType::BombBox:
        return 10000;
    case EntType::Dice:
        return 10000;
    case EntType::Cape:
        return 12000;
    case EntType::Damsel:
        return 12000;
    case EntType::Shotgun:
        return 15000;
    case EntType::JetPack:
        return 20000;
    default:
        return 0;
    }
}

std::uint32_t GetClassicShopPrice(EntType type_, int level_number) {
    std::uint32_t price = GetClassicShopBasePrice(type_);
    if (price == 0) {
        return 0;
    }
    if (level_number > 2) {
        price += (price / 100U) * 10U * static_cast<std::uint32_t>(level_number - 2);
    }
    return price;
}

const char* PickRightTikiArmFrameName() {
    switch (rng::RandomIntInclusive(0, 2)) {
    case 0:
        return "tiki_arm_right_0";
    case 1:
        return "tiki_arm_right_1";
    default:
        return "tiki_arm_right_2";
    }
}

const char* PickLeftTikiArmFrameName() {
    switch (rng::RandomIntInclusive(0, 2)) {
    case 0:
        return "tiki_arm_left_0";
    case 1:
        return "tiki_arm_left_1";
    default:
        return "tiki_arm_left_2";
    }
}

} // namespace

ResolvedRoom ResolveRoom(int room_code, int level_number, bool is_start_room, bool is_end_room, int room_code_above,
                         bool jungle_lake_active, const UVec2& room_size,
                         const Stage& existing_stage, const ClassicRoomTemplateDb& room_templates,
                         const GlyphMap& glyph_map, const ItemPoolDb& item_db,
                         const ShopConfigDb& shop_db) {
    const RoomTemplateSelection selection = SelectRoomTemplate(
        room_code, is_start_room, is_end_room, room_code_above, jungle_lake_active, room_templates);
    const std::string glyphs = ExpandObstacles(selection.glyphs);

    // Glyph markers are authored on the room's 10x8 tile grid. We anchor imported
    // ents and background stamps from that tile grid directly into our top-left
    // world-space positions, then only add explicit whole-tile layout offsets that
    // come from the room design itself, like the right altar half living one tile to
    // the right. We do not port HD sprite-origin offsets directly here.
    ResolvedRoom room;
    room.source_path = selection.source_path;
    if (glyphs.size() != static_cast<std::size_t>(room_size.x * room_size.y)) {
        throw std::runtime_error("Room glyph count does not match configured room_size");
    }
    room.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(room_size.y),
        std::vector<Tile>(static_cast<std::size_t>(room_size.x), Tile::Air));
    std::vector<std::size_t> pending_gold_idol_spawn_indices;
    std::vector<std::size_t> pending_giant_tiki_head_spawn_indices;
    const bool is_shop_room = IsShopRoomCode(room_code);
    const bool is_shop_area_room = IsShopAreaRoomCode(room_code);
    const bool is_craps_shop = is_shop_room && selection.shop_type == ShopType::Craps;
    const std::optional<std::size_t> shop_spawn_index = [&]() -> std::optional<std::size_t> {
        if (!is_shop_area_room) {
            return std::nullopt;
        }
        const Vec2 shop_size =
            Vec2::New(static_cast<float>(room_size.x * kTileSize),
                      static_cast<float>(room_size.y * kTileSize));
        room.ent_spawns.push_back(EntSpawn{
            .type_ = EntType::Shop,
            .pos = Vec2::New(0.0F, 0.0F),
            .size_override = shop_size,
            .ai_state_override = room_code == static_cast<int>(RoomCode::Vault)
                                     ? std::optional<EntAiState>(EntAiState::Disturbed)
                                     : std::nullopt,
            .exit_id = "",
        });
        return room.ent_spawns.size() - 1;
    }();
    const std::optional<std::size_t> craps_table_spawn_index = [&]() -> std::optional<std::size_t> {
        if (!is_craps_shop || !shop_spawn_index.has_value()) {
            return std::nullopt;
        }
        const Vec2 shop_size =
            Vec2::New(static_cast<float>(room_size.x * kTileSize),
                      static_cast<float>(room_size.y * kTileSize));
        room.ent_spawns.push_back(EntSpawn{
            .type_ = EntType::CrapsTable,
            .pos = Vec2::New(0.0F, 0.0F),
            .size_override = shop_size,
            .ent_a_spawn_index = *shop_spawn_index,
            .exit_id = "",
        });
        return room.ent_spawns.size() - 1;
    }();
    std::optional<std::size_t> craps_dice_spawn_index;
    const auto mark_shop_vandalism_tile = [&](int tile_x, int tile_y) {
        if (!is_shop_area_room || !shop_spawn_index.has_value()) {
            return;
        }
        room.tile_triggers.push_back(
            ents::shop::MakeShopVandalismTileTrigger(IVec2::New(tile_x, tile_y), *shop_spawn_index)
        );
    };

    for (int y = 0; y < static_cast<int>(room_size.y); ++y) {
        for (int x = 0; x < static_cast<int>(room_size.x); ++x) {
            const char glyph = glyphs[static_cast<std::size_t>(y * static_cast<int>(room_size.x) + x)];
            Tile tile = Tile::Air;
            const Vec2 tile_pos = Vec2::New(static_cast<float>(x * static_cast<int>(kTileSize)),
                                            static_cast<float>(y * static_cast<int>(kTileSize)));

            const GlyphRule* rule = glyph_map.Find(glyph);
            if (rule == nullptr) {
                throw std::runtime_error("Missing classic glyph rule for glyph: " +
                                         std::string(1, glyph));
            }

            const auto spawn_ent_at = [&](EntType ent_type, Vec2 pos) -> std::optional<std::size_t> {
                if (ent_type != EntType::None) {
                    room.ent_spawns.push_back(
                        EntSpawn{.type_ = ent_type, .pos = pos, .exit_id = ""});
                    return room.ent_spawns.size() - 1;
                }
                return std::nullopt;
            };
            const auto mark_spawn_buyable = [&](std::optional<std::size_t> spawn_index) {
                if (!is_shop_room || !shop_spawn_index.has_value() || !spawn_index.has_value()) {
                    return;
                }
                EntSpawn& spawn = room.ent_spawns[*spawn_index];
                const std::uint32_t price = GetClassicShopPrice(spawn.type_, level_number);
                if (price == 0) {
                    return;
                }
                spawn.buyable = true;
                spawn.buy_price = price;
                spawn.shop_owner_spawn_index = *shop_spawn_index;
            };
            const auto spawn_ent = [&](EntType ent_type) -> std::optional<std::size_t> {
                return spawn_ent_at(ent_type, tile_pos);
            };
            const auto roll_classic_ground_block = [&]() {
                return rng::RandomIntInclusive(1, 10) == 1
                           ? BlockTileForFamilyTile(existing_stage.border.left.tile)
                           : DirtTileForFamilyTile(existing_stage.border.left.tile);
            };
            const auto link_craps_prize = [&](std::optional<std::size_t> spawn_index) {
                if (!is_craps_shop || !craps_table_spawn_index.has_value() ||
                    !spawn_index.has_value()) {
                    return;
                }
                room.ent_spawns[*craps_table_spawn_index].ent_c_spawn_index = *spawn_index;
            };
            const auto spawn_shop_item = [&](EntType ent_type) {
                const std::optional<std::size_t> spawn_index = spawn_ent(ent_type);
                if (is_craps_shop) {
                    link_craps_prize(spawn_index);
                    return;
                }
                mark_spawn_buyable(spawn_index);
            };
            const auto spawn_ent_offset = [&](EntType ent_type, int dx_tiles,
                                                 int dy_tiles) -> std::optional<std::size_t> {
                constexpr int kSignedTileSize = static_cast<int>(kTileSize);
                return spawn_ent_at(ent_type,
                                       tile_pos + Vec2::New(static_cast<float>(dx_tiles * kSignedTileSize),
                                                            static_cast<float>(dy_tiles * kSignedTileSize)));
            };
            const auto set_room_tile = [&](int tile_x, int tile_y, Tile value) {
                if (tile_x < 0 || tile_y < 0 || tile_x >= static_cast<int>(room_size.x) ||
                    tile_y >= static_cast<int>(room_size.y)) {
                    return;
                }
                room.tiles[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)] =
                    value;
            };
            const auto room_tile_is_solid = [&](int tile_x, int tile_y) {
                if (tile_x < 0 || tile_y < 0 || tile_x >= static_cast<int>(room_size.x) ||
                    tile_y >= static_cast<int>(room_size.y)) {
                    return true;
                }
                return IsTileCollidable(room.tiles[static_cast<std::size_t>(tile_y)]
                                                  [static_cast<std::size_t>(tile_x)]);
            };

            tile = rule->tile.value_or(Tile::Air);

            if (rule->spawn != EntType::None) {
                if (is_craps_shop && rule->spawn == EntType::Dice &&
                    craps_dice_spawn_index.has_value()) {
                    room.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
                    continue;
                }
                const std::optional<std::size_t> spawn_index = spawn_ent(rule->spawn);
                if (is_craps_shop && rule->spawn == EntType::Dice &&
                    spawn_index.has_value() && craps_table_spawn_index.has_value()) {
                    craps_dice_spawn_index = *spawn_index;
                    room.ent_spawns[*craps_table_spawn_index].ent_b_spawn_index =
                        *spawn_index;
                }
                if (is_shop_area_room && rule->spawn == EntType::Shopkeeper &&
                    shop_spawn_index.has_value() && spawn_index.has_value()) {
                    room.ent_spawns[*spawn_index].ent_a_spawn_index = *shop_spawn_index;
                } else if (is_shop_room && !is_craps_shop &&
                           (rule->spawn == EntType::Dice || rule->spawn == EntType::Damsel)) {
                    mark_spawn_buyable(spawn_index);
                }
            }
            if (rule->spawn_chance != EntType::None &&
                rng::RandomIntInclusive(1, rule->chance_denominator) == 1) {
                spawn_ent(rule->spawn_chance);
            }
            if (!rule->spawn_random.empty()) {
                spawn_ent(PickWeightedEnt(rule->spawn_random));
            }
            if (!rule->patch_pool.empty()) {
                // Obstacle patch glyphs are expanded before rule resolution.
                tile = Tile::Air;
            }
            if (!rule->action.empty()) {
                const std::string& action = rule->action;
                if (action == "random_brick_or_block") {
                    tile = roll_classic_ground_block();
                } else if (action == "maybe_random_brick_or_block") {
                    tile = rng::RandomIntInclusive(1, 2) == 1 ? roll_classic_ground_block()
                                                              : Tile::Air;
                } else if (action == "jungle_ground") {
                    tile = Tile::Lush;
                } else if (action == "jungle_maybe_ground") {
                    tile = rng::RandomIntInclusive(1, 2) == 1 ? Tile::Lush : Tile::Air;
                } else if (action == "jungle_water_or_lush") {
                    tile = rng::RandomIntInclusive(1, 2) == 1 ? Tile::WaterSwim : Tile::Lush;
                } else if (action == "jungle_temple_or_lush") {
                    tile = rng::RandomIntInclusive(1, 2) == 1 ? Tile::TempleDirt : Tile::Lush;
                } else if (action == "jungle_water_and_lush") {
                    tile = Tile::WaterSwim;
                } else if (action == "jungle_water_and_maybe_lush") {
                    tile = Tile::WaterSwim;
                } else if (action == "ice_dark_or_ice") {
                    tile = rng::RandomIntInclusive(1, 10) == 1 ? Tile::IceBlock : Tile::Dark;
                } else if (action == "ice_maybe_dark_or_ice") {
                    if (rng::RandomIntInclusive(1, 2) == 1) {
                        tile = rng::RandomIntInclusive(1, 10) == 1 ? Tile::IceBlock : Tile::Dark;
                    }
                } else if (action == "temple_ground_rare_lush") {
                    if (rng::RandomIntInclusive(1, 100) == 1) {
                        tile = Tile::Lush;
                    } else if (rng::RandomIntInclusive(1, 10) == 1) {
                        tile = Tile::TempleBlock;
                    } else {
                        tile = Tile::TempleDirt;
                    }
                } else if (action == "temple_maybe_ground") {
                    if (rng::RandomIntInclusive(1, 2) == 1) {
                        tile = rng::RandomIntInclusive(1, 10) == 1 ? Tile::TempleBlock
                                                                   : Tile::TempleDirt;
                    }
                } else if (action == "temple_lava_or_ground") {
                    tile = rng::RandomIntInclusive(1, 2) == 1 ? Tile::Lava : Tile::TempleDirt;
                } else if (action == "maybe_lush") {
                    tile = rng::RandomIntInclusive(1, 2) == 1 ? Tile::Lush : Tile::Air;
                } else if (action == "maybe_ice_block") {
                    tile = rng::RandomIntInclusive(1, 2) == 1 ? Tile::IceBlock : Tile::Air;
                } else if (action == "maybe_spikes") {
                    tile = rng::RandomIntInclusive(1, 3) == 1 ? Tile::Spikes : Tile::Air;
                } else if (action == "entrance_or_exit") {
                    if (is_start_room) {
                        tile = Tile::Entrance;
                        room.ent_spawns.push_back(EntSpawn{
                            .type_ = EntType::Entrance,
                            .pos = tile_pos,
                            .exit_id = "",
                        });
                    } else if (is_end_room) {
                        room.ent_spawns.push_back(EntSpawn{
                            .type_ = EntType::BasicExit,
                            .pos = tile_pos,
                            .anim_id = aframe_ids::Exit,
                            .exit_id =
                                rule->exit_id.empty() ? std::string("default") : rule->exit_id,
                        });
                        tile = Tile::Air;
                    } else {
                        tile = Tile::Air;
                    }
                } else if (action == "shop_wall") {
                    tile = ShopWallTileForFamilyTile(existing_stage.border.left.tile);
                    mark_shop_vandalism_tile(x, y);
                } else if (action == "vault_wall_or_pushblock") {
                    if (rng::RandomIntInclusive(1, rule->chance_denominator) == 1) {
                        spawn_ent(EntType::Block);
                        tile = Tile::Air;
                    } else {
                        tile = ShopWallTileForFamilyTile(existing_stage.border.left.tile);
                        mark_shop_vandalism_tile(x, y);
                    }
                } else if (action == "smooth_wall") {
                    tile = SmoothWallTileForFamilyTile(existing_stage.border.left.tile);
                    mark_shop_vandalism_tile(x, y);
                } else if (action == "lush_shop_wall" || action == "lush_smooth_wall") {
                    tile = Tile::Lush;
                    mark_shop_vandalism_tile(x, y);
                } else if (action == "dark_shop_wall" || action == "dark_smooth_wall") {
                    tile = Tile::Dark;
                    mark_shop_vandalism_tile(x, y);
                } else if (action == "temple_shop_wall") {
                    tile = Tile::TempleDirt;
                    mark_shop_vandalism_tile(x, y);
                } else if (action == "lamp_or_red") {
                    spawn_ent(selection.shop_type == ShopType::Kissing ? EntType::LampRed
                                                                          : EntType::Lamp);
                } else if (action == "shop_damsel") {
                    spawn_shop_item(EntType::Damsel);
                } else if (action == "tree_growth") {
                    tile = Tile::Tree;
                    int trunk_y = y - 1;
                    for (int m = 0; m < 5; ++m) {
                        if (rng::RandomIntInclusive(0, m) > 2) {
                            break;
                        }
                        if (trunk_y < 0) {
                            break;
                        }
                        if (room_tile_is_solid(x, trunk_y) || room_tile_is_solid(x - 1, trunk_y) ||
                            room_tile_is_solid(x + 1, trunk_y)) {
                            break;
                        }
                        set_room_tile(x, trunk_y, Tile::Tree);
                        --trunk_y;
                    }
                } else if (action == "altar_pair") {
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::Altar,
                        .pos = tile_pos,
                        .anim_id = aframe_ids::AltarLeft,
                        .exit_id = "",
                    });
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::Altar,
                        .pos = tile_pos + Vec2::New(static_cast<float>(kTileSize), 0.0F),
                        .anim_id = aframe_ids::AltarRight,
                        .exit_id = "",
                    });
                } else if (action == "sac_altar") {
                    const std::size_t left_altar_spawn_index = room.ent_spawns.size();
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::SacAltar,
                        .pos = tile_pos,
                        .anim_id = aframe_ids::SacAltarLeft,
                        .exit_id = "",
                    });
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::SacAltar,
                        .pos = tile_pos + Vec2::New(static_cast<float>(kTileSize), 0.0F),
                        .anim_id = aframe_ids::SacAltarRight,
                        .ent_a_spawn_index = left_altar_spawn_index,
                        .exit_id = "",
                    });
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::SacAltarTopper,
                        .pos = tile_pos + Vec2::New(0.0F, -static_cast<float>(kTileSize)),
                        .anim_id = aframe_ids::SacAltarTopper,
                        .ent_a_spawn_index = left_altar_spawn_index,
                        .exit_id = "",
                    });
                    room.ent_spawns[left_altar_spawn_index].ent_a_spawn_index =
                        room.ent_spawns.size() - 1;
                } else if (action == "gold_idol") {
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::GoldIdol,
                        .pos = GetShrineIdolTopLeft(tile_pos),
                        .exit_id = "",
                    });
                    if (!pending_giant_tiki_head_spawn_indices.empty()) {
                        room.ent_spawns[pending_giant_tiki_head_spawn_indices.front()]
                            .ent_a_spawn_index = room.ent_spawns.size() - 1;
                        pending_giant_tiki_head_spawn_indices.erase(
                            pending_giant_tiki_head_spawn_indices.begin());
                    } else {
                        pending_gold_idol_spawn_indices.push_back(room.ent_spawns.size() - 1);
                    }
                } else if (action == "giant_tiki_head") {
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::GiantTikiHead,
                        .pos = tile_pos,
                        .exit_id = "",
                    });
                    if (!pending_gold_idol_spawn_indices.empty()) {
                        room.ent_spawns.back().ent_a_spawn_index =
                            pending_gold_idol_spawn_indices.front();
                        pending_gold_idol_spawn_indices.erase(
                            pending_gold_idol_spawn_indices.begin());
                    } else {
                        pending_giant_tiki_head_spawn_indices.push_back(room.ent_spawns.size() -
                                                                        1);
                    }
                    room.background_stamps.push_back(BackgroundStamp{
                        .anim_id = HashAFrameIdConstexpr("tiki_body"),
                        .pos = tile_pos + Vec2::New(0.0F, static_cast<float>(kTileSize * 2)),
                    });
                    room.background_stamps.push_back(BackgroundStamp{
                        .anim_id = HashAFrameIdConstexpr(PickRightTikiArmFrameName()),
                        .pos = tile_pos + Vec2::New(static_cast<float>(kTileSize * 2),
                                                    static_cast<float>(kTileSize * 2)),
                    });
                    room.background_stamps.push_back(BackgroundStamp{
                        .anim_id = HashAFrameIdConstexpr(PickLeftTikiArmFrameName()),
                        .pos = tile_pos + Vec2::New(-static_cast<float>(kTileSize),
                                                    static_cast<float>(kTileSize * 2)),
                    });
                } else if (action == "dice_sign_if_craps") {
                    if (selection.shop_type == ShopType::Craps) {
                        room.background_stamps.push_back(BackgroundStamp{
                            .anim_id = aframe_ids::DiceSign,
                            .pos = tile_pos,
                        });
                    }
                } else if (action == "high_end_shop_item") {
                    spawn_shop_item(
                        PickHighEndShopItemType(item_db, existing_stage, room.ent_spawns));
                } else if (action == "random_item") {
                    if (is_shop_room) {
                        const std::optional<std::size_t> item_spawn_index =
                            spawn_ent(PickUndergroundItemType(item_db, existing_stage));
                        if (is_craps_shop) {
                            link_craps_prize(item_spawn_index);
                        } else {
                            mark_spawn_buyable(item_spawn_index);
                        }
                    } else {
                        spawn_ent(PickUndergroundItemType(item_db, existing_stage));
                    }
                } else if (action == "wanted_poster") {
                    room.background_stamps.push_back(BackgroundStamp{
                        .anim_id = HashAFrameIdConstexpr("wanted_poster"),
                        .pos = tile_pos,
                        .condition = BackgroundStampCondition::Wanted,
                    });
                } else if (action == "shop_sign") {
                    spawn_ent(GetShopSignEntType(selection.shop_type, shop_db));
                } else if (action == "shop_item_slot") {
                    spawn_shop_item(PickShopItemType(selection.shop_type, existing_stage,
                                                     room.ent_spawns, item_db, shop_db));
                } else if (action == "ice_dark_ice_or_alien_floor") {
                    if (rng::RandomIntInclusive(1, 2) == 1) {
                        tile = rng::RandomIntInclusive(1, 10) == 1 ? Tile::IceBlock : Tile::Dark;
                    } else {
                        tile = Tile::AlienShip;
                    }
                } else if (action == "ice_alien_ship_top") {
                    spawn_ent(EntType::AlienShip);
                } else if (action == "ice_alien_ship_floor") {
                    tile = Tile::AlienShip;
                } else if (action == "ice_alien_ship_front_tall") {
                    spawn_ent(EntType::AlienShip);
                    spawn_ent_offset(EntType::AlienShip, 0, 1);
                    spawn_ent_offset(EntType::AlienShip, 0, 2);
                } else if (action == "ice_alien_ship_front_single") {
                    spawn_ent(EntType::AlienShip);
                } else if (action == "ice_alien_bg_tall" || action == "ice_alien_bg_mid") {
                    spawn_ent(EntType::AlienShip);
                } else if (action == "ice_jetpack_cache") {
                    tile = Tile::Dark;
                    spawn_ent(EntType::JetPack);
                } else if (action == "ice_moai_room") {
                    spawn_ent(EntType::Moai);
                    spawn_ent_offset(EntType::Moai2, 1, 0);
                    spawn_ent_offset(EntType::Moai3, 2, 0);
                    spawn_ent_offset(EntType::MoaiInside, 1, 1);
                    spawn_ent_offset(EntType::Door, 1, 3);
                    spawn_ent_offset(EntType::Crown, 1, 3);
                } else if (action == "ice_dark_invincible") {
                    tile = Tile::Dark;
                } else if (action == "temple_damsel_and_idol") {
                    spawn_ent(EntType::Damsel);
                    room.ent_spawns.push_back(EntSpawn{
                        .type_ = EntType::GoldIdol,
                        .pos = GetShrineIdolTopLeft(tile_pos),
                        .exit_id = "",
                    });
                } else if (action == "temple_ruby_block") {
                    tile = Tile::TempleDirt;
                    spawn_ent(EntType::RubyBig);
                } else if (action == "temple_treasure_roll") {
                    if (rng::RandomIntInclusive(1, 120) == 1) {
                        spawn_ent(EntType::RubyBig);
                    } else if (rng::RandomIntInclusive(1, 80) == 1) {
                        spawn_ent(EntType::SapphireBig);
                    } else if (rng::RandomIntInclusive(1, 60) == 1) {
                        spawn_ent(EntType::EmeraldBig);
                    } else {
                        spawn_ent(EntType::GoldBars);
                    }
                } else if (action == "temple_xoc_room") {
                    for (int l = 0; l < 6; ++l) {
                        for (int k = 0; k < 5; ++k) {
                            spawn_ent_offset(EntType::XocBlock, k, l);
                        }
                    }
                } else if (action == "maybe_solid_dirt") {
                    if (rng::RandomIntInclusive(1, 10) != 1 && rng::RandomIntInclusive(1, 2) == 1) {
                        tile = DirtTileForFamilyTile(existing_stage.border.left.tile);
                    }
                } else if (action == "maybe_snake_or_solid_dirt") {
                    if (rng::RandomIntInclusive(1, 10) == 1) {
                        spawn_ent(EntType::Snake);
                    } else if (rng::RandomIntInclusive(1, 2) == 1) {
                        tile = DirtTileForFamilyTile(existing_stage.border.left.tile);
                    }
                } else {
                    throw std::runtime_error("Unknown classic glyph action: " + action);
                }
            }

            room.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
        }
    }

    return room;
}

} // namespace splonks::stage_gen::classic

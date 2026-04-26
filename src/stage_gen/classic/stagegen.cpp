#include "stage_gen/classic/stagegen.hpp"

#include "stage_gen/classic/glyph_actions.hpp"
#include "stage_gen/classic/room_layout.hpp"
#include "stage_gen/classic/room_templates.hpp"
#include "stage_gen/classic/stage_passes.hpp"
#include "utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace splonks::stage_gen::classic {

namespace {

Stage GenerateClassicStage(int level_number, const StageGeneratorContext& context) {
    if (context.stage_config == nullptr) {
        throw std::runtime_error("Classic room graph generator missing stage config");
    }
    const StageConfig& stage_config = *context.stage_config;
    const QuestDefinition* quest = context.quest;
    const QuestStageDefinition* stage_def = context.stage_def;
    ValidateClassicRoomLayoutPasses(stage_config.layout_passes);
    const StageLayout layout = GenerateClassicRoomLayout(level_number, stage_config);
    const ClassicRoomTemplateDb room_templates = LoadClassicRoomTemplateDb(stage_config);
    const GlyphMap glyph_map = LoadGlyphMap(GetClassicQuestRootPath(), stage_config.glyphs_path);
    const ItemPoolDb item_db = LoadItemPoolDb(GetClassicQuestRootPath(), "pools/items.yaml");
    const ShopConfigDb shop_db = LoadShopConfigDb(GetClassicQuestRootPath(), "pools/shops.yaml");
    Stage stage;
    stage.quest_level_number = level_number;
    stage.stage_title = stage_config.title;
    if (quest != nullptr && stage_def != nullptr) {
        stage.quest_id = quest->id;
        stage.quest_stage_id = stage_def->id;
        stage.route_label = stage_def->route_label;
        for (const auto& [exit_id, exit] : stage_def->exits) {
            StageExit stage_exit;
            stage_exit.id = exit_id;
            stage_exit.target.target_stage_id = exit.target_stage_id;
            stage_exit.target.requirements.reserve(exit.requirements.size());
            for (const QuestExitRequirement& requirement : exit.requirements) {
                stage_exit.target.requirements.push_back(StageExitRequirement{
                    .flag = requirement.flag,
                    .expected = requirement.expected,
                });
            }
            stage.exits.push_back(std::move(stage_exit));
        }
    }
    if (stage_config.border_tile == Tile::Air) {
        throw std::runtime_error("Classic stage config missing border_tile: " + stage_config.id);
    }
    stage.border = Stage::MakeUniformBorder(stage_config.border_tile);
    stage.block_animation_id = stage_config.block_animation_id;
    std::vector<Tile> backwall_fill_tiles = stage_config.backwall_tiles;
    if (backwall_fill_tiles.empty()) {
        throw std::runtime_error("Classic stage config missing backwall_tiles: " + stage_config.id);
    }

    const UVec2 room_size = stage_config.room_size;
    const UVec2 layout_size = layout.layout_size;
    const UVec2 stage_shape = UVec2::New(room_size.x * layout_size.x, room_size.y * layout_size.y);
    std::vector<std::vector<Tile>> tiles(
        static_cast<std::size_t>(stage_shape.y),
        std::vector<Tile>(static_cast<std::size_t>(stage_shape.x), Tile::Air));
    std::vector<std::vector<int>> room_codes(
        static_cast<std::size_t>(layout_size.y),
        std::vector<int>(static_cast<std::size_t>(layout_size.x), 0));

    for (unsigned int room_y = 0; room_y < layout_size.y; ++room_y) {
        for (unsigned int room_x = 0; room_x < layout_size.x; ++room_x) {
            const int room_code = layout.room_codes[static_cast<std::size_t>(room_y)]
                                                   [static_cast<std::size_t>(room_x)];
            room_codes[static_cast<std::size_t>(room_y)][static_cast<std::size_t>(room_x)] =
                room_code;

            const bool is_start_room = room_x == static_cast<unsigned int>(layout.start_room.x) &&
                                       room_y == static_cast<unsigned int>(layout.start_room.y);
            const bool is_end_room = room_x == static_cast<unsigned int>(layout.end_room.x) &&
                                     room_y == static_cast<unsigned int>(layout.end_room.y);
            const int room_code_above = room_y == 0 ? -1 : layout.room_codes[static_cast<std::size_t>(room_y - 1)]
                                                                        [static_cast<std::size_t>(room_x)];

            ResolvedRoom room = ResolveRoom(room_code, level_number, is_start_room, is_end_room, room_code_above,
                                            layout.jungle_lake, room_size, stage, room_templates,
                                            glyph_map, item_db, shop_db);

            const UVec2 room_pos = UVec2::New(room_x * room_size.x, room_y * room_size.y);
            const Vec2 room_pos_wc = Vec2::New(static_cast<float>(room_pos.x * kTileSize),
                                               static_cast<float>(room_pos.y * kTileSize));
            stage.stagegen_annotations.push_back(StageGenAnnotation{
                .world_pos = room_pos_wc + Vec2::New(4.0F, 10.0F),
                .text = "room (" + std::to_string(room_x) + "," + std::to_string(room_y) +
                        "): " + room.source_path,
            });
            for (unsigned int tile_y = 0; tile_y < room_size.y; ++tile_y) {
                for (unsigned int tile_x = 0; tile_x < room_size.x; ++tile_x) {
                    const UVec2 tile_pos = room_pos + UVec2::New(tile_x, tile_y);
                    tiles[static_cast<std::size_t>(tile_pos.y)]
                         [static_cast<std::size_t>(tile_pos.x)] =
                             room.tiles[static_cast<std::size_t>(tile_y)]
                                       [static_cast<std::size_t>(tile_x)];
                }
            }
            const std::size_t room_spawn_base_index = stage.entity_spawns.size();
            for (StageEntitySpawn& spawn : room.entity_spawns) {
                spawn.pos += room_pos_wc;
                if (spawn.entity_a_spawn_index.has_value()) {
                    *spawn.entity_a_spawn_index += room_spawn_base_index;
                }
                if (spawn.entity_b_spawn_index.has_value()) {
                    *spawn.entity_b_spawn_index += room_spawn_base_index;
                }
                if (spawn.entity_c_spawn_index.has_value()) {
                    *spawn.entity_c_spawn_index += room_spawn_base_index;
                }
                if (spawn.entity_d_spawn_index.has_value()) {
                    *spawn.entity_d_spawn_index += room_spawn_base_index;
                }
                if (spawn.shop_owner_spawn_index.has_value()) {
                    *spawn.shop_owner_spawn_index += room_spawn_base_index;
                }
                stage.entity_spawns.push_back(std::move(spawn));
            }
            const IVec2 room_tile_offset = ToIVec2(room_pos);
            for (StageTileTrigger& trigger : room.tile_triggers) {
                trigger.tile_pos = trigger.tile_pos + room_tile_offset;
                if (trigger.target_spawn_index.has_value()) {
                    *trigger.target_spawn_index += room_spawn_base_index;
                }
                if (trigger.debug_label != nullptr) {
                    stage.stagegen_annotations.push_back(StageGenAnnotation{
                        .world_pos = ToVec2(trigger.tile_pos * static_cast<int>(kTileSize)) +
                                     Vec2::New(2.0F, 8.0F),
                        .text = trigger.debug_label,
                    });
                }
                stage.tile_triggers.push_back(std::move(trigger));
            }
            for (BackgroundStamp& stamp : room.background_stamps) {
                stamp.pos += room_pos_wc;
                stage.background_stamps.push_back(std::move(stamp));
            }
        }
    }

    stage.tiles = std::move(tiles);
    stage.FillBackwall(backwall_fill_tiles);
    stage.embedded_treasures = std::vector<std::vector<EmbeddedTreasure>>(
        stage.tiles.size(),
        std::vector<EmbeddedTreasure>(stage.tiles.empty() ? 0U : stage.tiles.front().size()));
    stage.rooms = std::move(room_codes);
    stage.path = layout.path;
    stage.gravity = 0.3F;
    stage.camera_clamp_margin = ToVec2(room_size * kTileSize) / 2.0F;
    AddStageGenAnnotation(stage, "layout: start (" + std::to_string(layout.start_room.x) + "," +
                                     std::to_string(layout.start_room.y) + ") exit (" +
                                     std::to_string(layout.end_room.x) + "," +
                                     std::to_string(layout.end_room.y) + ") path " +
                                     std::to_string(layout.path.size()) + " rooms");
    for (const StagePassConfig& pass : stage_config.stage_passes) {
        RunStagePass(stage, level_number, pass, item_db);
    }
    return stage;
}

} // namespace

Stage GenerateStage(const QuestDefinition& quest, const QuestStageDefinition& stage_def,
                    const StageConfig& stage_config) {
    if (stage_config.generator != "classic_room_graph") {
        throw std::runtime_error("Unsupported stage generator: " + stage_config.generator);
    }
    StageGeneratorContext context;
    context.quest = &quest;
    context.stage_def = &stage_def;
    context.stage_config = &stage_config;
    return GenerateClassicStage(stage_def.level_number, context);
}

const char* GetRoomCodeDebugLabel(int room_code) {
    return GetClassicRoomCodeDebugLabel(room_code);
}

} // namespace splonks::stage_gen::classic

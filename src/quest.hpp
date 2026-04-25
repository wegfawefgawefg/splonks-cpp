#pragma once

#include "entity/core_types.hpp"
#include "math_types.hpp"
#include "stage.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace splonks {

struct StagePassConfig {
    std::string name;
    bool enabled = true;
    std::unordered_map<std::string, std::string> properties;

    int GetInt(std::string_view key, int fallback) const;
    bool GetBool(std::string_view key, bool fallback) const;
};


struct WeightedEntityEntry {
    EntityType entity_type = EntityType::None;
    int weight = 1;
};

struct EntityPoolConfig {
    std::string id;
    bool unique = false;
    std::vector<WeightedEntityEntry> entries;
};

struct ItemPoolDb {
    std::unordered_map<std::string, EntityPoolConfig> pools;

    const EntityPoolConfig* FindPool(std::string_view id) const;
};

struct ShopTypeConfig {
    std::string id;
    EntityType sign = EntityType::None;
    std::string item_pool;
    int item_slots = 0;
};

struct ShopConfigDb {
    std::unordered_map<std::string, ShopTypeConfig> shop_types;

    const ShopTypeConfig* FindShopType(std::string_view id) const;
};

struct GlyphRule {
    char glyph = '\0';
    std::optional<Tile> tile;
    std::string action;
    std::string patch_pool;
    EntityType spawn = EntityType::None;
    EntityType spawn_chance = EntityType::None;
    int chance_denominator = 1;
    std::string exit_id;
    std::vector<WeightedEntityEntry> spawn_random;
};

struct GlyphMap {
    std::unordered_map<char, GlyphRule> rules;

    const GlyphRule* Find(char glyph) const;
};

struct StageConfig {
    std::string id;
    std::string title;
    std::string generator;
    UVec2 room_size = UVec2::New(0, 0);
    UVec2 layout_size = UVec2::New(0, 0);
    UVec2 path_layout_size = UVec2::New(0, 0);
    std::string glyphs_path;
    Tile border_tile = Tile::Air;
    std::vector<Tile> backwall_tiles;
    FrameDataId block_animation_id = frame_data_ids::CaveBlock;
    std::unordered_map<std::string, std::string> room_pools;
    std::vector<StagePassConfig> layout_passes;
    std::vector<StagePassConfig> stage_passes;
};

struct QuestExitRequirement {
    std::string flag;
    bool expected = true;
};

struct StageExitDefinition {
    std::string target_stage_id;
    std::vector<QuestExitRequirement> requirements;
};

struct QuestStageDefinition {
    std::string id;
    std::string route_label;
    std::string stage_file;
    int level_number = 0;
    std::unordered_map<std::string, StageExitDefinition> exits;
};

struct QuestDefinition {
    std::string id;
    std::string title;
    std::string start_stage;
    std::string quest_state;
    std::vector<QuestStageDefinition> stages;

    const QuestStageDefinition* FindStage(std::string_view stage_id) const;
};

struct ClassicQuestState {
    bool made_black_market = false;
    bool has_udjat_eye = false;
    bool made_moai = false;
    bool has_hedjet = false;
    bool has_sceptre = false;
    bool has_book_of_dead = false;
};

enum class QuestId {
    None,
    Classic,
};

struct QuestState {
    QuestId quest_id = QuestId::None;
    ClassicQuestState classic;
};

struct StageGeneratorContext {
    const QuestDefinition* quest = nullptr;
    const QuestStageDefinition* stage_def = nullptr;
    const StageConfig* stage_config = nullptr;
    const QuestState* quest_state = nullptr;
    std::vector<StageGenAnnotation>* annotations = nullptr;
};

const char* QuestIdToString(QuestId quest_id);
QuestId QuestIdFromString(std::string_view id);

const char* GetClassicQuestRootPath();
QuestDefinition LoadQuestDefinition(const std::string& quest_yaml_path);
StageConfig LoadStageConfig(const std::string& quest_root_path, const std::string& stage_file_path);
GlyphMap LoadGlyphMap(const std::string& quest_root_path, const std::string& glyph_file_path);
ItemPoolDb LoadItemPoolDb(const std::string& quest_root_path, const std::string& pool_file_path);
ShopConfigDb LoadShopConfigDb(const std::string& quest_root_path, const std::string& shop_file_path);

const StagePassConfig* FindPassConfig(
    const std::vector<StagePassConfig>& passes,
    std::string_view name
);
bool IsPassEnabled(
    const std::vector<StagePassConfig>& passes,
    std::string_view name,
    bool fallback
);
int GetPassInt(
    const std::vector<StagePassConfig>& passes,
    std::string_view pass_name,
    std::string_view key,
    int fallback
);
bool GetPassBool(
    const std::vector<StagePassConfig>& passes,
    std::string_view pass_name,
    std::string_view key,
    bool fallback
);

} // namespace splonks

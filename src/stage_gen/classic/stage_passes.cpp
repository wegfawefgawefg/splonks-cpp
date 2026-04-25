#include "stage_gen/classic/stage_passes.hpp"

#include "stage_gen/classic/ambient_passes.hpp"
#include "stage_gen/classic/treasure_passes.hpp"

#include <array>
#include <stdexcept>
#include <string_view>

namespace splonks::stage_gen::classic {

void AddStageGenAnnotation(Stage& stage, const std::string& text) {
    const float line = static_cast<float>(stage.stagegen_annotations.size() % 24U);
    stage.stagegen_annotations.push_back(StageGenAnnotation{
        .world_pos = Vec2::New(4.0F, 20.0F + (line * 8.0F)),
        .text = text,
    });
}

namespace {

using StagePassFn = void (*)(Stage&, int, const StagePassConfig&, const ItemPoolDb&);

struct StagePassDefinition {
    std::string_view name;
    StagePassFn run = nullptr;
};

void RunConvertExitTilesStagePass(Stage& stage, int, const StagePassConfig&, const ItemPoolDb&) {
    ConvertExitTilesToBasicExitSpawns(stage);
}

void RunEmbeddedTreasureStagePass(Stage& stage, int, const StagePassConfig&,
                                  const ItemPoolDb& item_db) {
    AddMinesEmbeddedTreasure(stage, item_db);
}

void RunFloorTreasureStagePass(Stage& stage, int level_number, const StagePassConfig&,
                               const ItemPoolDb&) {
    AddMinesTreasure(stage, level_number);
}

void RunUdjatKeyChestStagePass(Stage& stage, int level_number, const StagePassConfig& pass,
                               const ItemPoolDb&) {
    if (level_number >= pass.GetInt("min_level_number", 2)) {
        AddUdjatKeyChest(stage);
    }
}

void RunArrowTrapConversionStagePass(Stage& stage, int, const StagePassConfig&, const ItemPoolDb&) {
    ConvertBlocksToArrowTraps(stage);
}

void RunAmbientMinesEntitiesStagePass(Stage& stage, int, const StagePassConfig&,
                                      const ItemPoolDb&) {
    AddAmbientMinesEntities(stage);
}

void RunAmbientJungleEntitiesStagePass(Stage& stage, int, const StagePassConfig&,
                                       const ItemPoolDb&) {
    AddAmbientJungleEntities(stage, false);
}

void RunAmbientBlackMarketEntitiesStagePass(Stage& stage, int, const StagePassConfig&,
                                            const ItemPoolDb&) {
    AddAmbientJungleEntities(stage, true);
}

void RunAmbientHauntedCastleEntitiesStagePass(Stage& stage, int, const StagePassConfig&,
                                              const ItemPoolDb&) {
    AddAmbientTempleEntities(stage);
}

void RunAmbientIceEntitiesStagePass(Stage& stage, int, const StagePassConfig&, const ItemPoolDb&) {
    AddAmbientIceEntities(stage);
}

void RunAmbientTempleEntitiesStagePass(Stage& stage, int, const StagePassConfig&,
                                       const ItemPoolDb&) {
    AddAmbientTempleEntities(stage);
}

void RunAmbientCityOfGoldEntitiesStagePass(Stage& stage, int, const StagePassConfig&,
                                           const ItemPoolDb&) {
    AddAmbientTempleEntities(stage);
}

void RunAmbientOlmecEntitiesStagePass(Stage& stage, int, const StagePassConfig&,
                                      const ItemPoolDb&) {
    AddAmbientOlmecEntities(stage);
}

void RunBranchExitStagePass(Stage& stage, int, const StagePassConfig& pass, const ItemPoolDb&) {
    AddBranchExit(stage, pass);
}

constexpr std::array<StagePassDefinition, 14> kStagePasses = {{
    {"convert_exit_tiles", RunConvertExitTilesStagePass},
    {"branch_exit", RunBranchExitStagePass},
    {"embedded_treasure", RunEmbeddedTreasureStagePass},
    {"floor_treasure", RunFloorTreasureStagePass},
    {"udjat_key_chest", RunUdjatKeyChestStagePass},
    {"arrow_trap_conversion", RunArrowTrapConversionStagePass},
    {"ambient_mines_entities", RunAmbientMinesEntitiesStagePass},
    {"ambient_jungle_entities", RunAmbientJungleEntitiesStagePass},
    {"ambient_black_market_entities", RunAmbientBlackMarketEntitiesStagePass},
    {"ambient_haunted_castle_entities", RunAmbientHauntedCastleEntitiesStagePass},
    {"ambient_ice_entities", RunAmbientIceEntitiesStagePass},
    {"ambient_temple_entities", RunAmbientTempleEntitiesStagePass},
    {"ambient_city_of_gold_entities", RunAmbientCityOfGoldEntitiesStagePass},
    {"ambient_olmec_entities", RunAmbientOlmecEntitiesStagePass},
}};

const StagePassDefinition* FindStagePass(std::string_view name) {
    for (const StagePassDefinition& pass : kStagePasses) {
        if (pass.name == name) {
            return &pass;
        }
    }
    return nullptr;
}

} // namespace

void RunStagePass(Stage& stage, int level_number, const StagePassConfig& pass,
                  const ItemPoolDb& item_db) {
    if (!pass.enabled) {
        AddStageGenAnnotation(stage, "stage pass skipped: " + pass.name);
        return;
    }

    const StagePassDefinition* definition = FindStagePass(pass.name);
    if (definition == nullptr) {
        throw std::runtime_error("Unknown classic stage pass: " + pass.name);
    }

    const std::size_t spawns_before = stage.entity_spawns.size();
    const std::size_t background_before = stage.background_stamps.size();
    definition->run(stage, level_number, pass, item_db);

    AddStageGenAnnotation(
        stage, "stage pass: " + pass.name + " spawns +" +
                   std::to_string(stage.entity_spawns.size() - spawns_before) + " stamps +" +
                   std::to_string(stage.background_stamps.size() - background_before));
}

} // namespace splonks::stage_gen::classic

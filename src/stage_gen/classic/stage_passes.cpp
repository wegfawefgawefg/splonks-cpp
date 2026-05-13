#include "stage_gen/classic/stage_passes.hpp"

#include "stage_gen/classic/ambient_passes.hpp"
#include "stage_gen/classic/treasure_passes.hpp"
#include "utils.hpp"

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

using StagePassFn = void (*)(Stage&, int, const StagePassConfig&, const ItemPoolDb&, QuestState*);

struct StagePassDefinition {
    std::string_view name;
    StagePassFn run = nullptr;
};

void RunConvertExitTilesStagePass(Stage& stage, int, const StagePassConfig&, const ItemPoolDb&,
                                  QuestState*) {
    ConvertExitTilesToBasicExitSpawns(stage);
}

void RunEmbeddedTreasureStagePass(Stage& stage, int, const StagePassConfig&,
                                  const ItemPoolDb& item_db, QuestState*) {
    AddMinesEmbeddedTreasure(stage, item_db);
}

void RunFloorTreasureStagePass(Stage& stage, int level_number, const StagePassConfig&,
                               const ItemPoolDb&, QuestState*) {
    AddMinesTreasure(stage, level_number);
}

int UdjatChanceDenominatorForLevel(int level_number) {
    if (level_number == 2) {
        return 3;
    }
    if (level_number == 3) {
        return 2;
    }
    if (level_number == 4) {
        return 1;
    }
    return 0;
}

void RunUdjatKeyChestStagePass(Stage& stage, int level_number, const StagePassConfig& pass,
                               const ItemPoolDb&, QuestState* quest_state) {
    if (level_number < pass.GetInt("min_level_number", 2)) {
        AddStageGenAnnotation(stage, "udjat skipped: level too low");
        return;
    }
    if (quest_state != nullptr &&
        (quest_state->classic.made_udjat_eye || quest_state->classic.has_udjat_eye)) {
        AddStageGenAnnotation(stage, "udjat skipped: already made");
        return;
    }

    const int chance_denominator = UdjatChanceDenominatorForLevel(level_number);
    if (chance_denominator <= 0) {
        AddStageGenAnnotation(stage, "udjat skipped: no classic roll for level");
        return;
    }
    if (chance_denominator > 1 && rng::RandomIntInclusive(1, chance_denominator) != 1) {
        AddStageGenAnnotation(stage, "udjat skipped: chance miss");
        return;
    }

    if (AddUdjatKeyChest(stage)) {
        if (quest_state != nullptr) {
            quest_state->classic.made_udjat_eye = true;
        }
        AddStageGenAnnotation(stage, "udjat key chest placed");
    } else {
        AddStageGenAnnotation(stage, "udjat skipped: no placement");
    }
}

void RunArrowTrapConversionStagePass(Stage& stage, int, const StagePassConfig& pass,
                                     const ItemPoolDb&, QuestState*) {
    ConvertBlocksToArrowTraps(stage, pass.GetInt("chance_denominator", 4));
}

void RunAmbientMinesEntsStagePass(Stage& stage, int, const StagePassConfig&,
                                      const ItemPoolDb&, QuestState*) {
    AddAmbientMinesEnts(stage);
}

void RunAmbientJungleEntsStagePass(Stage& stage, int, const StagePassConfig&,
                                       const ItemPoolDb&, QuestState*) {
    AddAmbientJungleEnts(stage, false);
}

void RunAmbientBlackMarketEntsStagePass(Stage& stage, int, const StagePassConfig&,
                                            const ItemPoolDb&, QuestState*) {
    AddAmbientJungleEnts(stage, true);
}

void RunAmbientHauntedCastleEntsStagePass(Stage& stage, int, const StagePassConfig&,
                                              const ItemPoolDb&, QuestState*) {
    AddAmbientTempleEnts(stage);
}

void RunAmbientIceEntsStagePass(Stage& stage, int, const StagePassConfig&, const ItemPoolDb&,
                                    QuestState*) {
    AddAmbientIceEnts(stage);
}

void RunAmbientTempleEntsStagePass(Stage& stage, int, const StagePassConfig&,
                                       const ItemPoolDb&, QuestState*) {
    AddAmbientTempleEnts(stage);
}

void RunAmbientCityOfGoldEntsStagePass(Stage& stage, int, const StagePassConfig&,
                                           const ItemPoolDb&, QuestState*) {
    AddAmbientTempleEnts(stage);
}

void RunAmbientOlmecEntsStagePass(Stage& stage, int, const StagePassConfig&,
                                      const ItemPoolDb&, QuestState*) {
    AddAmbientOlmecEnts(stage);
}

void RunBranchExitStagePass(Stage& stage, int, const StagePassConfig& pass, const ItemPoolDb&,
                            QuestState*) {
    AddBranchExit(stage, pass);
}

constexpr std::array<StagePassDefinition, 14> kStagePasses = {{
    {"convert_exit_tiles", RunConvertExitTilesStagePass},
    {"branch_exit", RunBranchExitStagePass},
    {"embedded_treasure", RunEmbeddedTreasureStagePass},
    {"floor_treasure", RunFloorTreasureStagePass},
    {"udjat_key_chest", RunUdjatKeyChestStagePass},
    {"arrow_trap_conversion", RunArrowTrapConversionStagePass},
    {"ambient_mines_ents", RunAmbientMinesEntsStagePass},
    {"ambient_jungle_ents", RunAmbientJungleEntsStagePass},
    {"ambient_black_market_ents", RunAmbientBlackMarketEntsStagePass},
    {"ambient_haunted_castle_ents", RunAmbientHauntedCastleEntsStagePass},
    {"ambient_ice_ents", RunAmbientIceEntsStagePass},
    {"ambient_temple_ents", RunAmbientTempleEntsStagePass},
    {"ambient_city_of_gold_ents", RunAmbientCityOfGoldEntsStagePass},
    {"ambient_olmec_ents", RunAmbientOlmecEntsStagePass},
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
                  const ItemPoolDb& item_db, QuestState* quest_state) {
    if (!pass.enabled) {
        AddStageGenAnnotation(stage, "stage pass skipped: " + pass.name);
        return;
    }

    const StagePassDefinition* definition = FindStagePass(pass.name);
    if (definition == nullptr) {
        throw std::runtime_error("Unknown classic stage pass: " + pass.name);
    }

    const std::size_t spawns_before = stage.ent_spawns.size();
    const std::size_t background_before = stage.background_stamps.size();
    definition->run(stage, level_number, pass, item_db, quest_state);

    AddStageGenAnnotation(
        stage, "stage pass: " + pass.name + " spawns +" +
                   std::to_string(stage.ent_spawns.size() - spawns_before) + " stamps +" +
                   std::to_string(stage.background_stamps.size() - background_before));
}

} // namespace splonks::stage_gen::classic

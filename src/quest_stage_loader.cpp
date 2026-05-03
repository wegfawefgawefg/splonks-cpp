#include "quest_stage_loader.hpp"

#include "quest.hpp"
#include "stage_gen/classic/stagegen.hpp"
#include "stage_init.hpp"
#include "state.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace splonks {

namespace {

void ApplyClassicQuestStageEntryFlags(QuestState& quest_state, std::string_view stage_id) {
    if (quest_state.quest_id != QuestId::Classic) {
        return;
    }
    if (stage_id == "classic_black_market") {
        quest_state.classic.made_black_market = true;
    }
}

bool GetClassicQuestFlag(const ClassicQuestState& quest_state, const std::string& flag) {
    if (flag == "made_black_market") return quest_state.made_black_market;
    if (flag == "made_udjat_eye") return quest_state.made_udjat_eye;
    if (flag == "has_udjat_eye") return quest_state.has_udjat_eye;
    if (flag == "made_moai") return quest_state.made_moai;
    if (flag == "has_hedjet") return quest_state.has_hedjet;
    if (flag == "has_sceptre") return quest_state.has_sceptre;
    if (flag == "has_book_of_dead") return quest_state.has_book_of_dead;
    return false;
}

bool LoadClassicQuestStage(
    State& state,
    std::string_view quest_stage_id,
    bool preserve_player_state,
    std::optional<std::uint32_t> seed
) {
    if (seed.has_value()) {
        rng::SetSeed(*seed);
    }

    const QuestDefinition quest =
        LoadQuestDefinition(std::string(GetClassicQuestRootPath()) + "/quest.yaml");
    const QuestStageDefinition* stage_def = quest.FindStage(quest_stage_id);
    if (stage_def == nullptr) {
        if (quest_stage_id == "classic_win") {
            state.stage = Stage::NewBlank();
            state.quest_state.quest_id = QuestId::Classic;
            state.SetMode(Mode::Win);
            return true;
        }
        return false;
    }

    const StageConfig stage_config =
        LoadStageConfig(GetClassicQuestRootPath(), stage_def->stage_file);
    const QuestId loaded_quest_id = QuestIdFromString(quest.id);
    if (!preserve_player_state || state.quest_state.quest_id != loaded_quest_id) {
        state.quest_state = QuestState{
            .quest_id = loaded_quest_id,
            .classic = ClassicQuestState{},
        };
    } else {
        state.quest_state.quest_id = loaded_quest_id;
    }
    state.stage = stage_gen::classic::GenerateStage(
        quest,
        *stage_def,
        stage_config,
        &state.quest_state
    );
    state.stage.generation_seed = seed;
    ApplyClassicQuestStageEntryFlags(state.quest_state, stage_def->id);
    InitStage(state, preserve_player_state);
    state.depth = static_cast<std::uint32_t>(std::max(0, stage_def->level_number - 1));
    return true;
}

} // namespace

bool QuestExitRequirementsMet(const QuestState& quest_state, const StageExitTarget& target) {
    if (quest_state.quest_id != QuestId::Classic) {
        return target.requirements.empty();
    }
    for (const StageExitRequirement& requirement : target.requirements) {
        if (GetClassicQuestFlag(quest_state.classic, requirement.flag) != requirement.expected) {
            return false;
        }
    }
    return true;
}

bool LoadQuestStage(
    State& state,
    std::string_view quest_id,
    std::string_view quest_stage_id,
    bool preserve_player_state
) {
    return LoadQuestStage(state, quest_id, quest_stage_id, preserve_player_state, std::nullopt);
}

bool LoadQuestStage(
    State& state,
    std::string_view quest_id,
    std::string_view quest_stage_id,
    bool preserve_player_state,
    std::optional<std::uint32_t> seed
) {
    if (quest_id == "classic") {
        return LoadClassicQuestStage(state, quest_stage_id, preserve_player_state, seed);
    }
    return false;
}

} // namespace splonks

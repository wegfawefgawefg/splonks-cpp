#pragma once

#include <string_view>
#include <cstdint>
#include <optional>

namespace splonks {

struct QuestState;
struct StageExitTarget;
struct State;

bool QuestExitRequirementsMet(const QuestState& quest_state, const StageExitTarget& target);
bool LoadQuestStage(
    State& state,
    std::string_view quest_id,
    std::string_view quest_stage_id,
    bool preserve_player_state
);
bool LoadQuestStage(
    State& state,
    std::string_view quest_id,
    std::string_view quest_stage_id,
    bool preserve_player_state,
    std::optional<std::uint32_t> seed
);

} // namespace splonks

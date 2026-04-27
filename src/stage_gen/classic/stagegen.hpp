#pragma once

#include "stage.hpp"
#include "quest.hpp"

namespace splonks::stage_gen::classic {

Stage GenerateStage(const QuestDefinition& quest, const QuestStageDefinition& stage_def, const StageConfig& stage_config);
Stage GenerateStage(const QuestDefinition& quest, const QuestStageDefinition& stage_def,
                    const StageConfig& stage_config, QuestState* quest_state);
const char* GetRoomCodeDebugLabel(int room_code);

} // namespace splonks::stage_gen::classic

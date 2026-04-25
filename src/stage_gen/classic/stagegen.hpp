#pragma once

#include "stage.hpp"
#include "quest.hpp"

namespace splonks::stage_gen::classic {

Stage GenerateStage(const QuestDefinition& quest, const QuestStageDefinition& stage_def, const StageConfig& stage_config);
const char* GetRoomCodeDebugLabel(int room_code);

} // namespace splonks::stage_gen::classic

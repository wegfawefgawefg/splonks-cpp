#pragma once

#include "stage.hpp"

namespace splonks::stage_gen::hd_mines {

bool UsesHdMinesGenerator(StageType stage_type);
Stage GenerateStage(StageType stage_type);
const char* GetRoomCodeDebugLabel(int room_code);

} // namespace splonks::stage_gen::hd_mines

#pragma once

#include "state.hpp"

namespace splonks {

Stage MakeHangTestStage(const HangTestLevelConfig& config);
Stage MakeStompTestStage();
Stage MakeBorderTestStage(const BorderTestLevelConfig& config);
Stage MakeMazeDoorTestStage(MazeDoorTestRoom room);
Stage MakeBowlingTestStage();
Stage MakeOpposingBodySmackStage();
Stage MakeBoulderTestStage();
Stage MakeMovingPlatformTestStage();
Stage MakeAudioTestStage();
Stage MakeParachuteTestStage();
Stage MakeSacAltarTestStage();
Stage MakeArrowTrapTestStage();
Stage MakeSpikeTestStage();
Stage MakeTrapDoorTestStage();
Stage MakeMonkeyTestStage();
Stage MakeCrusherTrapTestStage();
Stage MakeWaterPiranhaTestStage();

void InitHangTestStage(State& state);
void InitStompTestStage(State& state);
void InitBorderTestStage(State& state);
void InitMazeDoorTestStage(State& state, bool preserve_player_state);
void InitBowlingTestStage(State& state);
void InitOpposingBodySmackStage(State& state);
void InitBoulderTestStage(State& state);
void InitMovingPlatformTestStage(State& state);
void InitAudioTestStage(State& state);
void InitParachuteTestStage(State& state);
void InitSacAltarTestStage(State& state);
void InitArrowTrapTestStage(State& state);
void InitSpikeTestStage(State& state);
void InitTrapDoorTestStage(State& state);
void InitMonkeyTestStage(State& state);
void InitCrusherTrapTestStage(State& state);
void InitWaterPiranhaTestStage(State& state);

} // namespace splonks

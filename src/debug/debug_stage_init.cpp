#include "stage_init.hpp"

#include "debug/debug_stage_builders.hpp"
#include "debug/shop_test_stage.hpp"
#include "stage_acoustics.hpp"

namespace splonks {

void InitDebugLevel(State& state, bool preserve_player_state) {
    state.depth = 0;
    state.sac_altar_favor = 0;
    state.sac_altar_reward_tier = 0;
    state.respawn_target = StageLoadTarget::ForDebugLevel(state.debug_level.kind);
    switch (state.debug_level.kind) {
    case DebugLevelKind::HangTest:
        state.stage = MakeHangTestStage(state.debug_level.hang_test);
        InitHangTestStage(state);
        break;
    case DebugLevelKind::StompTest:
        state.stage = MakeStompTestStage();
        InitStompTestStage(state);
        break;
    case DebugLevelKind::BorderTest:
        state.stage = MakeBorderTestStage(state.debug_level.border_test);
        InitBorderTestStage(state);
        break;
    case DebugLevelKind::MazeDoorTest:
        state.stage = MakeMazeDoorTestStage(state.debug_level.maze_door_test.room);
        state.respawn_target = StageLoadTarget::ForDebugLevel(
            DebugLevelKind::MazeDoorTest,
            static_cast<std::uint8_t>(MazeDoorTestRoom::RoomA)
        );
        InitMazeDoorTestStage(state, preserve_player_state);
        break;
    case DebugLevelKind::BowlingTest:
        state.stage = MakeBowlingTestStage();
        InitBowlingTestStage(state);
        break;
    case DebugLevelKind::OpposingBodySmack:
        state.stage = MakeOpposingBodySmackStage();
        InitOpposingBodySmackStage(state);
        break;
    case DebugLevelKind::BoulderTest:
        state.stage = MakeBoulderTestStage();
        InitBoulderTestStage(state);
        break;
    case DebugLevelKind::MovingPlatformTest:
        state.stage = MakeMovingPlatformTestStage();
        InitMovingPlatformTestStage(state);
        break;
    case DebugLevelKind::AudioTest:
        state.stage = MakeAudioTestStage();
        InitAudioTestStage(state);
        break;
    case DebugLevelKind::ShopTest:
        state.stage = MakeShopTestStage();
        InitShopTestStage(state);
        break;
    case DebugLevelKind::ParachuteTest:
        state.stage = MakeParachuteTestStage();
        InitParachuteTestStage(state);
        break;
    case DebugLevelKind::SacAltarTest:
        state.stage = MakeSacAltarTestStage();
        InitSacAltarTestStage(state);
        break;
    case DebugLevelKind::ArrowTrapTest:
        state.stage = MakeArrowTrapTestStage();
        InitArrowTrapTestStage(state);
        break;
    case DebugLevelKind::SpikeTest:
        state.stage = MakeSpikeTestStage();
        InitSpikeTestStage(state);
        break;
    case DebugLevelKind::TrapDoorTest:
        state.stage = MakeTrapDoorTestStage();
        InitTrapDoorTestStage(state);
        break;
    case DebugLevelKind::MonkeyTest:
        state.stage = MakeMonkeyTestStage();
        InitMonkeyTestStage(state);
        break;
    case DebugLevelKind::CrusherTrapTest:
        state.stage = MakeCrusherTrapTestStage();
        InitCrusherTrapTestStage(state);
        break;
    }

    state.stage_acoustics = StageAcoustics::New();
}

} // namespace splonks

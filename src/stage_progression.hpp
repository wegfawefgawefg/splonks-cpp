#pragma once

#include "stage.hpp"

#include <optional>
#include <cstdint>
#include <array>
#include <string_view>

namespace splonks {

struct State;

enum class DebugLevelKind {
    HangTest,
    StompTest,
    BorderTest,
    MazeDoorTest,
    BowlingTest,
    OpposingBodySmack,
    BoulderTest,
    MovingPlatformTest,
    AudioTest,
    ShopTest,
    ParachuteTest,
    SacAltarTest,
    ArrowTrapTest,
    SpikeTest,
    TrapDoorTest,
    MonkeyTest,
    CrusherTrapTest,
    WaterPiranhaTest,
};

constexpr int kDebugLevelKindCount = static_cast<int>(DebugLevelKind::WaterPiranhaTest) + 1;

enum class StageLoadTargetKind {
    StageType,
    DebugLevel,
    QuestStage,
};

struct StageLoadTarget {
    StageLoadTargetKind kind = StageLoadTargetKind::StageType;
    StageType stage_type = StageType::Blank;
    DebugLevelKind debug_level = DebugLevelKind::HangTest;
    std::uint8_t debug_variant = 0;
    std::array<char, 32> quest_id{};
    std::array<char, 64> quest_stage_id{};

    static StageLoadTarget ForStageType(StageType stage_type);
    static StageLoadTarget ForDebugLevel(DebugLevelKind debug_level, std::uint8_t debug_variant = 0);
    static StageLoadTarget ForQuestStage(std::string_view quest_id, std::string_view quest_stage_id);
};

struct StageTransitionTarget {
    StageLoadTarget destination = StageLoadTarget::ForStageType(StageType::Blank);
    bool preserve_player_state = true;
};

const char* GetDebugLevelKindName(DebugLevelKind kind);
void QueueStageTransition(State& state, const StageTransitionTarget& target);
void QueueStageTransition(
    State& state,
    const StageLoadTarget& destination,
    bool preserve_player_state
);
void QueueRespawnTransition(State& state);
bool IsStageExitAllowed(const State& state, StageExitId exit_id);
void QueueStageExitTransition(State& state, StageExitId exit_id);
void ApplyPendingStageTransition(State& state);

} // namespace splonks

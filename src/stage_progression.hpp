#pragma once

#include "stage.hpp"

#include <optional>

namespace splonks {

struct State;

enum class DebugLevelKind {
    SplkMines1,
    HangTest,
    StompTest,
    BorderTest,
};

enum class StageLoadTargetKind {
    StageType,
    DebugLevel,
};

struct StageLoadTarget {
    StageLoadTargetKind kind = StageLoadTargetKind::StageType;
    StageType stage_type = StageType::SplkMines1;
    DebugLevelKind debug_level = DebugLevelKind::SplkMines1;

    static StageLoadTarget ForStageType(StageType stage_type);
    static StageLoadTarget ForDebugLevel(DebugLevelKind debug_level);
};

struct StageTransitionTarget {
    StageLoadTarget destination = StageLoadTarget::ForStageType(StageType::SplkMines1);
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
void ApplyPendingStageTransition(State& state);

} // namespace splonks

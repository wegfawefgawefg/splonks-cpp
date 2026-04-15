#include "stage_progression.hpp"

#include "stage_init.hpp"
#include "state.hpp"

#include <algorithm>

namespace splonks {

StageLoadTarget StageLoadTarget::ForStageType(StageType stage_type) {
    StageLoadTarget target;
    target.kind = StageLoadTargetKind::StageType;
    target.stage_type = stage_type;
    return target;
}

StageLoadTarget StageLoadTarget::ForDebugLevel(DebugLevelKind debug_level, std::uint8_t debug_variant) {
    StageLoadTarget target;
    target.kind = StageLoadTargetKind::DebugLevel;
    target.debug_level = debug_level;
    target.debug_variant = debug_variant;
    return target;
}

const char* GetDebugLevelKindName(DebugLevelKind kind) {
    switch (kind) {
    case DebugLevelKind::SplkMines1:
        return "SplkMines1";
    case DebugLevelKind::HangTest:
        return "HangTest";
    case DebugLevelKind::StompTest:
        return "StompTest";
    case DebugLevelKind::BorderTest:
        return "BorderTest";
    case DebugLevelKind::MazeDoorTest:
        return "MazeDoorTest";
    case DebugLevelKind::BowlingTest:
        return "BowlingTest";
    case DebugLevelKind::OpposingBodySmack:
        return "OpposingBodySmack";
    }

    return "Unknown";
}

void QueueStageTransition(State& state, const StageTransitionTarget& target) {
    state.pending_stage_transition = target;
}

void QueueStageTransition(
    State& state,
    const StageLoadTarget& destination,
    bool preserve_player_state
) {
    state.pending_stage_transition = StageTransitionTarget{
        .destination = destination,
        .preserve_player_state = preserve_player_state,
    };
}

void QueueRespawnTransition(State& state) {
    QueueStageTransition(state, state.respawn_target, false);
}

void ApplyPendingStageTransition(State& state) {
    if (!state.pending_stage_transition.has_value()) {
        return;
    }

    const StageTransitionTarget target = *state.pending_stage_transition;
    state.pending_stage_transition.reset();

    switch (target.destination.kind) {
    case StageLoadTargetKind::StageType:
        state.stage = Stage::New(target.destination.stage_type);
        InitStage(state, target.preserve_player_state);
        break;
    case StageLoadTargetKind::DebugLevel:
        state.debug_level.kind = target.destination.debug_level;
        if (target.destination.debug_level == DebugLevelKind::MazeDoorTest) {
            const std::uint8_t room_index = std::min<std::uint8_t>(target.destination.debug_variant, 2);
            state.debug_level.maze_door_test.room = static_cast<MazeDoorTestRoom>(room_index);
        }
        InitDebugLevel(state, target.preserve_player_state);
        break;
    }
}

} // namespace splonks

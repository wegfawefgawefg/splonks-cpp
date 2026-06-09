#include "stage_progression.hpp"

#include "ent.hpp"
#include "quest_stage_loader.hpp"
#include "stage_init.hpp"
#include "state.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cstring>
#include <string>

namespace splonks {

template <std::size_t N>
void CopyTargetString(std::array<char, N>& target, std::string_view value) {
    target.fill('\0');
    const std::size_t copy_size = std::min(value.size(), N - 1);
    std::memcpy(target.data(), value.data(), copy_size);
}


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

StageLoadTarget StageLoadTarget::ForQuestStage(std::string_view quest_id, std::string_view quest_stage_id) {
    StageLoadTarget target;
    target.kind = StageLoadTargetKind::QuestStage;
    CopyTargetString(target.quest_id, quest_id);
    CopyTargetString(target.quest_stage_id, quest_stage_id);
    return target;
}

const char* GetDebugLevelKindName(DebugLevelKind kind) {
    switch (kind) {
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
    case DebugLevelKind::BoulderTest:
        return "BoulderTest";
    case DebugLevelKind::MovingPlatformTest:
        return "MovingPlatformTest";
    case DebugLevelKind::AudioTest:
        return "AudioTest";
    case DebugLevelKind::ShopTest:
        return "ShopTest";
    case DebugLevelKind::ParachuteTest:
        return "ParachuteTest";
    case DebugLevelKind::SacAltarTest:
        return "SacAltarTest";
    case DebugLevelKind::ArrowTrapTest:
        return "ArrowTrapTest";
    case DebugLevelKind::SpikeTest:
        return "SpikeTest";
    case DebugLevelKind::TrapDoorTest:
        return "TrapDoorTest";
    case DebugLevelKind::MonkeyTest:
        return "MonkeyTest";
    case DebugLevelKind::CrusherTrapTest:
        return "CrusherTrapTest";
    case DebugLevelKind::WaterPiranhaTest:
        return "WaterPiranhaTest";
    case DebugLevelKind::LightingStressTest:
        return "LightingStressTest";
    }

    return "Unknown";
}

std::uint32_t MakeRandomStageSeed() {
    const std::uint32_t seed = rng::RandomU32();
    return seed == 0 ? 1U : seed;
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

bool IsStageExitAllowed(const State& state, StageExitId exit_id) {
    if (state.stage.exits.empty()) {
        return true;
    }

    const StageExit* const exit = state.stage.GetExit(exit_id);
    return exit != nullptr && QuestExitRequirementsMet(state.quest_state, exit->target);
}

void QueueLegacyStageTypeExitTransition(State& state) {
    if (state.stage.stage_type == StageType::Test1) {
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Test1), true);
        return;
    }
    state.stage = Stage::NewBlank();
    state.mode = Mode::Win;
}

void QueueStageExitTransition(State& state, StageExitId exit_id) {
    if (state.stage.exits.empty()) {
        QueueLegacyStageTypeExitTransition(state);
        return;
    }

    const StageExit* const exit = state.stage.GetExit(exit_id);
    if (exit == nullptr || !QuestExitRequirementsMet(state.quest_state, exit->target)) {
        return;
    }

    state.depth += 1;
    QueueStageTransition(
        state,
        StageLoadTarget::ForQuestStage(state.stage.quest_id, exit->target.target_stage_id),
        true
    );
}

void ApplyPendingStageTransition(State& state) {
    if (!state.pending_stage_transition.has_value()) {
        return;
    }

    const StageTransitionTarget target = *state.pending_stage_transition;
    state.pending_stage_transition.reset();

    switch (target.destination.kind) {
    case StageLoadTargetKind::StageType:
        state.stage = Stage::New(target.destination.stage_type, state.stagegen_drng);
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
    case StageLoadTargetKind::QuestStage:
        (void)LoadQuestStage(
            state,
            target.destination.quest_id.data(),
            target.destination.quest_stage_id.data(),
            target.preserve_player_state,
            target.seed
        );
        break;
    }
}

std::optional<sim::FxVec2> FindStageEntranceSpawnPos(const State& state) {
    for (unsigned int y = 0; y < state.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < state.stage.GetTileWidth(); ++x) {
            if (state.stage.GetTile(x, y) == Tile::Entrance) {
                return sim::PixelVec2(static_cast<int>(x) * static_cast<int>(kTileSize),
                                      static_cast<int>(y) * static_cast<int>(kTileSize));
            }
        }
    }

    for (const Ent& ent : state.ents.ents) {
        if (ent.active && ent.type_ == EntType::Entrance) {
            return ent.pos;
        }
    }

    return std::nullopt;
}

std::vector<VID> ResetStageEntrancePres(State& state) {
    std::vector<VID> changed_ents;
    for (Ent& ent : state.ents.ents) {
        if (!ent.active || ent.type_ != EntType::Entrance) {
            continue;
        }
        ent.counter_a = sim::Scalar::zero();
        ent.counter_b = sim::Scalar::zero();
        changed_ents.push_back(ent.vid);
    }
    return changed_ents;
}

} // namespace splonks

#include "debug/playback_internal.hpp"

#include "ent/spec.hpp"
#include "aframe.hpp"
#include "sim/fxp.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace splonks::debug_playback_internal {

const char* ModeToString(Mode mode) {
    switch (mode) {
    case Mode::Title:
        return "Title";
    case Mode::Settings:
        return "Settings";
    case Mode::VideoSettings:
        return "VideoSettings";
    case Mode::UiSettings:
        return "UiSettings";
    case Mode::PostFxSettings:
        return "PostFxSettings";
    case Mode::LightingSettings:
        return "LightingSettings";
    case Mode::Playing:
        return "Playing";
    case Mode::StageTransition:
        return "StageTransition";
    case Mode::GameOver:
        return "GameOver";
    case Mode::Win:
        return "Win";
    }
    return "Unknown";
}

const char* DebugLevelKindToString(DebugLevelKind kind) {
    return GetDebugLevelKindName(kind);
}

const char* EntTypeToString(EntType type) {
    return GetEntTypeName(type);
}

const char* DisplayStateToString(EntDisplayState state) {
    switch (state) {
    case EntDisplayState::Neutral:
        return "Neutral";
    case EntDisplayState::NeutralHolding:
        return "NeutralHolding";
    case EntDisplayState::Walk:
        return "Walk";
    case EntDisplayState::WalkHolding:
        return "WalkHolding";
    case EntDisplayState::Fly:
        return "Fly";
    case EntDisplayState::Dead:
        return "Dead";
    case EntDisplayState::Stunned:
        return "Stunned";
    case EntDisplayState::Climbing:
        return "Climbing";
    case EntDisplayState::Hanging:
        return "Hanging";
    case EntDisplayState::Falling:
        return "Falling";
    case EntDisplayState::EmoteDab:
        return "EmoteDab";
    case EntDisplayState::EmoteBald:
        return "EmoteBald";
    }
    return "Unknown";
}

const char* ConditionToString(EntCondition condition) {
    switch (condition) {
    case EntCondition::Normal:
        return "Normal";
    case EntCondition::Dead:
        return "Dead";
    case EntCondition::Stunned:
        return "Stunned";
    }
    return "Unknown";
}

const char* AiStateToString(EntAiState ai_state) {
    switch (ai_state) {
    case EntAiState::Idle:
        return "Idle";
    case EntAiState::Disturbed:
        return "Disturbed";
    case EntAiState::Patrolling:
        return "Patrolling";
    case EntAiState::Pursuing:
        return "Pursuing";
    case EntAiState::Returning:
        return "Returning";
    }
    return "Unknown";
}

const char* SideToString(Side facing) {
    switch (facing) {
    case Side::Left:
        return "Left";
    case Side::Right:
        return "Right";
    }
    return "Unknown";
}

bool ExportRecordingToTextFile(
    const DebugPlayback& debug,
    const Graphics& graphics,
    std::string* status_out
) {
    if (debug.file_path[0] == '\0') {
        if (status_out != nullptr) {
            *status_out = "No file path set.";
        }
        return false;
    }

    std::ofstream out(debug.file_path.data(), std::ios::trunc);
    if (!out.is_open()) {
        if (status_out != nullptr) {
            *status_out = "Failed to open file for text export.";
        }
        return false;
    }

    out << "splonks recording text export\n";
    out << "snapshots: " << debug.recorded_snapshots.size() << "\n\n";

    for (std::size_t snapshot_index = 0; snapshot_index < debug.recorded_snapshots.size();
         ++snapshot_index) {
        const GameplaySnapshot& snapshot = debug.recorded_snapshots[snapshot_index];
        out << "snapshot " << snapshot_index << "\n";
        out << "  mode: " << ModeToString(snapshot.mode) << "\n";
        out << "  scene_frame: " << snapshot.scene_frame << "\n";
        out << "  frame: " << snapshot.frame << "\n";
        out << "  stage_frame: " << snapshot.stage_frame << "\n";
        out << "  stage_type: " << static_cast<int>(snapshot.stage.stage_type) << "\n";
        out << "  points: " << snapshot.points << "\n";
        out << "  deaths: " << snapshot.deaths << "\n";
        out << "  quest_id: " << QuestIdToString(snapshot.quest_state.quest_id) << "\n";
        out << "  quest_stage_id: " << snapshot.stage.quest_stage_id << "\n";
        out << "  play_cam_pos: (" << snapshot.play_cam_pos.x << ", " << snapshot.play_cam_pos.y
            << ")\n";

        std::size_t active_count = 0;
        for (const Ent& ent : snapshot.ents.ents) {
            if (ent.active) {
                active_count += 1;
            }
        }
        out << "  active_ents: " << active_count << "\n";

        for (std::size_t ent_id = 0; ent_id < snapshot.ents.ents.size();
             ++ent_id) {
            const Ent& ent = snapshot.ents.ents[ent_id];
            if (!ent.active) {
                continue;
            }

            out << "  ent " << ent_id << "\n";
            out << "    type: " << EntTypeToString(ent.type_) << "\n";
            out << "    condition: " << ConditionToString(ent.condition) << "\n";
            out << "    ai_state: " << AiStateToString(ent.ai_state) << "\n";
            out << "    facing: " << SideToString(ent.facing) << "\n";
            out << "    grounded: " << (ent.grounded ? "true" : "false") << "\n";
            out << "    climbing: " << (ent.IsClimbing() ? "true" : "false") << "\n";
            out << "    holding: " << (ent.holding ? "true" : "false") << "\n";
            out << "    coyote_time: " << ent.coyote_time << "\n";
            out << "    fall_timer: " << ent.fall_timer << "\n";
            const FVec2 ent_pos = ent.GetRenderPos();
            const FVec2 ent_vel = ent.GetRenderVel();
            const FVec2 ent_acc = ent.GetRenderAcc();
            out << "    pos: (" << ent_pos.x << ", " << ent_pos.y << ")\n";
            out << "    vel: (" << ent_vel.x << ", " << ent_vel.y << ")\n";
            out << "    acc: (" << ent_acc.x << ", " << ent_acc.y << ")\n";
            const FVec2 ent_size = ent.GetSize();
            out << "    size: (" << ent_size.x << ", " << ent_size.y << ")\n";
            out << "    health: " << ent.health << "\n";
            out << "    money: " << ent.money << "\n";

            if (ent.aframe_animator.HasAnim()) {
                const AFrameAnim* anim =
                    graphics.aframe_db.FindAnim(ent.aframe_animator.anim_id);
                if (anim != nullptr) {
                    out << "    anim: " << anim->name << "\n";
                    out << "    anim_frame: " << ent.aframe_animator.current_frame << "\n";
                    out << "    anim_time: "
                        << sim::ToRenderScalar(ent.aframe_animator.current_time) << "\n";
                    const AFrame* aframe = graphics.aframe_db.FindFrame(
                        ent.aframe_animator.anim_id,
                        ent.aframe_animator.current_frame
                    );
                    if (aframe != nullptr) {
                        out << "    frame_duration: " << aframe->duration << "\n";
                        out << "    sample_rect: (" << aframe->sample_rect.x << ", "
                            << aframe->sample_rect.y << ", " << aframe->sample_rect.w
                            << ", " << aframe->sample_rect.h << ")\n";
                        out << "    draw_offset: (" << aframe->draw_offset.x << ", "
                            << aframe->draw_offset.y << ")\n";
                        out << "    pbox: (" << aframe->pbox.x << ", " << aframe->pbox.y
                            << ", " << aframe->pbox.w << ", " << aframe->pbox.h << ")\n";
                        out << "    cbox: (" << aframe->cbox.x << ", " << aframe->cbox.y
                            << ", " << aframe->cbox.w << ", " << aframe->cbox.h << ")\n";
                    }
                }
            }
        }

        out << "\n";
    }

    if (!out.good()) {
        if (status_out != nullptr) {
            *status_out = "Text export failed.";
        }
        return false;
    }

    if (status_out != nullptr) {
        char buffer[128];
        std::snprintf(
            buffer,
            sizeof(buffer),
            "Exported %zu snapshots as text.",
            debug.recorded_snapshots.size()
        );
        *status_out = buffer;
    }
    return true;
}

} // namespace splonks::debug_playback_internal

namespace splonks {

bool ConvertRecordingFileToText(
    const std::string& input_path,
    const std::string& output_path,
    const AFrameDb& aframe_db,
    std::string* status_out
) {
    DebugPlayback debug = DebugPlayback::New();
    std::strncpy(debug.file_path.data(), input_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    if (!debug_playback_internal::LoadRecordingFromFile(debug, status_out)) {
        return false;
    }

    Graphics graphics{};
    graphics.aframe_db = aframe_db;
    std::strncpy(debug.file_path.data(), output_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    return debug_playback_internal::ExportRecordingToTextFile(debug, graphics, status_out);
}

} // namespace splonks

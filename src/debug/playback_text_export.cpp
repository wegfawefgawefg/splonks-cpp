#include "debug/playback_internal.hpp"

#include "entity/archetype.hpp"
#include "frame_data.hpp"

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
    switch (kind) {
    case DebugLevelKind::Cave1:
        return "Cave1";
    case DebugLevelKind::HangTest:
        return "HangTest";
    case DebugLevelKind::StompTest:
        return "StompTest";
    case DebugLevelKind::BorderTest:
        return "BorderTest";
    }
    return "Unknown";
}

const char* EntityTypeToString(EntityType type) {
    return GetEntityTypeName(type);
}

const char* DisplayStateToString(EntityDisplayState state) {
    switch (state) {
    case EntityDisplayState::Neutral:
        return "Neutral";
    case EntityDisplayState::NeutralHolding:
        return "NeutralHolding";
    case EntityDisplayState::Walk:
        return "Walk";
    case EntityDisplayState::WalkHolding:
        return "WalkHolding";
    case EntityDisplayState::Fly:
        return "Fly";
    case EntityDisplayState::Dead:
        return "Dead";
    case EntityDisplayState::Stunned:
        return "Stunned";
    case EntityDisplayState::Climbing:
        return "Climbing";
    case EntityDisplayState::Hanging:
        return "Hanging";
    case EntityDisplayState::Falling:
        return "Falling";
    }
    return "Unknown";
}

const char* ConditionToString(EntityCondition condition) {
    switch (condition) {
    case EntityCondition::Normal:
        return "Normal";
    case EntityCondition::Dead:
        return "Dead";
    case EntityCondition::Stunned:
        return "Stunned";
    }
    return "Unknown";
}

const char* AiStateToString(EntityAiState ai_state) {
    switch (ai_state) {
    case EntityAiState::Idle:
        return "Idle";
    case EntityAiState::Pursuing:
        return "Pursuing";
    case EntityAiState::Returning:
        return "Returning";
    }
    return "Unknown";
}

const char* LeftOrRightToString(LeftOrRight facing) {
    switch (facing) {
    case LeftOrRight::Left:
        return "Left";
    case LeftOrRight::Right:
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
        out << "  play_cam_pos: (" << snapshot.play_cam_pos.x << ", " << snapshot.play_cam_pos.y
            << ")\n";

        std::size_t active_count = 0;
        for (const Entity& entity : snapshot.entity_manager.entities) {
            if (entity.active) {
                active_count += 1;
            }
        }
        out << "  active_entities: " << active_count << "\n";

        for (std::size_t entity_id = 0; entity_id < snapshot.entity_manager.entities.size();
             ++entity_id) {
            const Entity& entity = snapshot.entity_manager.entities[entity_id];
            if (!entity.active) {
                continue;
            }

            out << "  entity " << entity_id << "\n";
            out << "    type: " << EntityTypeToString(entity.type_) << "\n";
            out << "    condition: " << ConditionToString(entity.condition) << "\n";
            out << "    ai_state: " << AiStateToString(entity.ai_state) << "\n";
            out << "    facing: " << LeftOrRightToString(entity.facing) << "\n";
            out << "    grounded: " << (entity.grounded ? "true" : "false") << "\n";
            out << "    climbing: " << (entity.IsClimbing() ? "true" : "false") << "\n";
            out << "    holding: " << (entity.holding ? "true" : "false") << "\n";
            out << "    coyote_time: " << entity.coyote_time << "\n";
            out << "    pos: (" << entity.pos.x << ", " << entity.pos.y << ")\n";
            out << "    vel: (" << entity.vel.x << ", " << entity.vel.y << ")\n";
            out << "    acc: (" << entity.acc.x << ", " << entity.acc.y << ")\n";
            out << "    size: (" << entity.size.x << ", " << entity.size.y << ")\n";
            out << "    health: " << entity.health << "\n";
            out << "    money: " << entity.money << "\n";
            out << "    bombs: " << entity.bombs << "\n";
            out << "    ropes: " << entity.ropes << "\n";

            if (entity.frame_data_animator.HasAnimation()) {
                const FrameDataAnimation* animation =
                    graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
                if (animation != nullptr) {
                    out << "    animation: " << animation->name << "\n";
                    out << "    animation_frame: " << entity.frame_data_animator.current_frame << "\n";
                    out << "    animation_time: " << entity.frame_data_animator.current_time << "\n";
                    const FrameData* frame_data = graphics.frame_data_db.FindFrame(
                        entity.frame_data_animator.animation_id,
                        entity.frame_data_animator.current_frame
                    );
                    if (frame_data != nullptr) {
                        out << "    frame_duration: " << frame_data->duration << "\n";
                        out << "    sample_rect: (" << frame_data->sample_rect.x << ", "
                            << frame_data->sample_rect.y << ", " << frame_data->sample_rect.w
                            << ", " << frame_data->sample_rect.h << ")\n";
                        out << "    draw_offset: (" << frame_data->draw_offset.x << ", "
                            << frame_data->draw_offset.y << ")\n";
                        out << "    pbox: (" << frame_data->pbox.x << ", " << frame_data->pbox.y
                            << ", " << frame_data->pbox.w << ", " << frame_data->pbox.h << ")\n";
                        out << "    cbox: (" << frame_data->cbox.x << ", " << frame_data->cbox.y
                            << ", " << frame_data->cbox.w << ", " << frame_data->cbox.h << ")\n";
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
    const FrameDataDb& frame_data_db,
    std::string* status_out
) {
    DebugPlayback debug = DebugPlayback::New();
    std::strncpy(debug.file_path.data(), input_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    if (!debug_playback_internal::LoadRecordingFromFile(debug, status_out)) {
        return false;
    }

    Graphics graphics{};
    graphics.frame_data_db = frame_data_db;
    std::strncpy(debug.file_path.data(), output_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    return debug_playback_internal::ExportRecordingToTextFile(debug, graphics, status_out);
}

} // namespace splonks

#include "debug/playback_internal.hpp"

#include "audio_acoustics.hpp"
#include "imgui_layer.hpp"
#include "quest_stage_loader.hpp"
#include "stage_init.hpp"
#include "stage_rotation.hpp"
#include "stage_wrap.hpp"
#include "utils.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace splonks::debug_playback_internal {

namespace {

constexpr float kMinTimeScale = 0.01F;
constexpr float kMaxTimeScale = 2.0F;
constexpr int kMinSnapshots = 1;
constexpr int kMaxSnapshots = 20000;

const char* StageRotationWrapPolicyName(StageRotationWrapPolicy policy) {
    switch (policy) {
    case StageRotationWrapPolicy::DoNotChangeWrap:
        return "Do Not Change Wrap";
    case StageRotationWrapPolicy::SwapXYWrap:
        return "Swap X/Y Wrap";
    }
    return "Unknown";
}

int DefaultVoidDeathYForStage(const Stage& stage) {
    return static_cast<int>(stage.GetHeight() + 8 * kTileSize);
}

std::optional<int> GetClassicQuestStageIndex(const QuestDefinition& quest, const State& state) {
    if (state.stage.quest_id != quest.id) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < quest.stages.size(); ++i) {
        if (quest.stages[i].id == state.stage.quest_stage_id) {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

std::vector<std::string> BuildQuestStageLabels(const QuestDefinition& quest) {
    std::vector<std::string> labels;
    labels.reserve(quest.stages.size());
    for (const QuestStageDefinition& stage : quest.stages) {
        labels.push_back(stage.route_label + " " + stage.id);
    }
    return labels;
}

void ResetStageDebugState(State& state, Graphics& graphics) {
    graphics.ResetTileVariations();
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
}

void LoadClassicQuestStage(
    State& state,
    Graphics& graphics,
    const QuestDefinition& quest,
    int stage_index,
    bool preserve_player_state,
    std::optional<std::uint32_t> seed
) {
    if (quest.stages.empty()) {
        return;
    }
    stage_index = std::clamp(stage_index, 0, static_cast<int>(quest.stages.size()) - 1);
    if (seed.has_value()) {
        rng::SetSeed(*seed);
    }
    (void)LoadQuestStage(
        state,
        quest.id,
        quest.stages[static_cast<std::size_t>(stage_index)].id,
        preserve_player_state
    );
    ResetStageDebugState(state, graphics);
}

bool DrawTileCombo(const char* label, Tile& tile) {
    bool changed = false;
    if (ImGui::BeginCombo(label, TileToString(tile))) {
        for (std::size_t i = 0; i < kTileCount; ++i) {
            const Tile candidate = static_cast<Tile>(i);
            const bool selected = candidate == tile;
            if (ImGui::Selectable(TileToString(candidate), selected)) {
                tile = candidate;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void CopyBorderConfigToStage(const BorderTestLevelConfig& config, Stage& stage) {
    stage.border.left.tile = config.left_tile;
    stage.border.right.tile = config.right_tile;
    stage.border.top.tile = config.top_tile;
    stage.border.bottom.tile = config.bottom_tile;
    stage.border.void_death_y = config.void_death_y;
}

void ApplyBorderTestWrapConfig(State& state, Graphics& graphics) {
    const BorderTestLevelConfig& border_test = state.debug_level.border_test;
    ApplyToroidalWrapSettings(
        state,
        graphics,
        border_test.wrap_x,
        border_test.wrap_y,
        static_cast<unsigned int>(std::max(0, border_test.wrap_padding_tiles)),
        border_test.camera_clamp_enabled
    );
}

} // namespace

void DrawDebugMenu(DebugPlayback& debug, State& state) {
    if (!debug.ui_visible) {
        SyncDebugUiSettings(debug, state);
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(12.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Menu", &debug.ui_visible)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Window Toggles");
    ImGui::Checkbox("Playback", &debug.playback_window_visible);
    ImGui::Checkbox("Level", &debug.level_window_visible);
    ImGui::Checkbox("Border", &debug.border_window_visible);
    ImGui::Checkbox("Entities", &debug.entity_inspector_visible);
    ImGui::Checkbox("Overlay", &debug.entity_annotations_visible);
    ImGui::Checkbox("Shake Brush", &debug.shake_brush_window_visible);
    ImGui::Checkbox("Audio Brush", &debug.audio_brush_window_visible);
    ImGui::Checkbox("Audio Settings", &debug.audio_settings_window_visible);
    ImGui::Checkbox("UI Settings", &debug.ui_settings_window_visible);
    ImGui::Checkbox("Camera Settings", &debug.camera_settings_window_visible);
    ImGui::Checkbox("Performance", &debug.performance_settings_window_visible);
    ImGui::Checkbox("Player Tuning", &debug.player_tuning_window_visible);
    ImGui::Checkbox("Post FX Settings", &debug.post_fx_settings_window_visible);
    ImGui::Checkbox("Lighting Settings", &debug.lighting_settings_window_visible);
    ImGui::Checkbox("Graphics Settings", &debug.graphics_settings_window_visible);
    ImGui::Separator();
    ImGui::TextUnformatted("Shortcuts");
    ImGui::TextUnformatted("F1: Toggle all ImGui");
    ImGui::TextUnformatted("F2: Toggle debug menu");
    ImGui::TextUnformatted("Overlay toggles live in the Overlay window.");
    ImGui::TextUnformatted("Shake brush controls live in the Shake Brush window.");
    ImGui::TextUnformatted("Audio brush controls live in the Audio Brush window.");
    ImGui::TextUnformatted("Persisted acoustics tuning lives in the Audio Settings window.");
    ImGui::Separator();
    ImGui::Text("Playback Active: %s", debug.playback_active ? "true" : "false");
    ImGui::Text("Selected Entity: %zu", debug.selected_entity_id);
    ImGui::Text("Entity P/C boxes: %s", state.debug_overlay.show_entity_collision_boxes ? "on" : "off");
    ImGui::Text("Chunk bounds: %s", state.debug_overlay.show_chunk_boundaries ? "on" : "off");

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawSimulationControls(DebugPlayback& debug, State& state, Audio& audio, Graphics& graphics) {
    if (!debug.playback_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(12.0F, 120.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Playback", &debug.playback_window_visible)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Mode: %s", ModeToString(state.mode));
    ImGui::Text("Scene Frame: %u", state.scene_frame);
    ImGui::Text("Game Frame: %u", state.frame);
    ImGui::Text("Stage Frame: %u", state.stage_frame);
    ImGui::Text("Snapshots: %zu", debug.recorded_snapshots.size());
    ImGui::Text("Playback: %s", debug.playback_active ? "On" : "Off");
    ImGui::Separator();

    ImGui::SliderFloat("Time Scale", &debug.time_scale, kMinTimeScale, kMaxTimeScale, "%.2fx");
    debug.time_scale = std::clamp(debug.time_scale, kMinTimeScale, kMaxTimeScale);
    ImGui::Checkbox("Pause Live Simulation", &debug.pause_live_simulation);
    if (ImGui::Button("Step One Tick")) {
        debug.step_live_simulation_once = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("1x")) {
        debug.time_scale = 1.0F;
    }
    ImGui::SameLine();
    if (ImGui::Button("0.25x")) {
        debug.time_scale = 0.25F;
    }
    ImGui::SameLine();
    if (ImGui::Button("0.10x")) {
        debug.time_scale = 0.10F;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("World Rotation");
    ImGui::Text("Active: %s", state.stage_rotation.active ? "true" : "false");
    if (state.stage_rotation.active) {
        ImGui::Text(
            "Frame: %d / %d",
            state.stage_rotation.elapsed_frames,
            state.stage_rotation.duration_frames
        );
    }
    if (ImGui::BeginCombo(
            "Wrap Policy",
            StageRotationWrapPolicyName(state.stage_rotation.wrap_policy))) {
        for (int i = 0; i < 2; ++i) {
            const StageRotationWrapPolicy policy = static_cast<StageRotationWrapPolicy>(i);
            const bool selected = policy == state.stage_rotation.wrap_policy;
            if (ImGui::Selectable(StageRotationWrapPolicyName(policy), selected)) {
                state.stage_rotation.wrap_policy = policy;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Rotate Stage 90 CW")) {
        StartStageRotation(state, graphics, audio, 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Rotate Stage 90 CCW")) {
        StartStageRotation(state, graphics, audio, -1);
    }

    ImGui::Separator();
    ImGui::InputInt("Max Snapshots", &debug.max_snapshots);
    debug.max_snapshots = std::clamp(debug.max_snapshots, kMinSnapshots, kMaxSnapshots);

    if (!debug.playback_active) {
        if (!debug.recording) {
            if (ImGui::Button("Start Recording")) {
                StartRecording(debug, state, graphics);
            }
        } else {
            if (ImGui::Button("Stop Recording")) {
                StopRecording(debug);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Recording")) {
            debug.recorded_snapshots.clear();
            debug.playback_index = 0;
            debug.recording = false;
        }
        if (!debug.recorded_snapshots.empty()) {
            if (ImGui::Button("Enter Playback")) {
                EnterPlayback(debug, state, graphics);
            }
        }
    } else {
        if (ImGui::Button("Exit Playback")) {
            ExitPlayback(debug, state, graphics);
        }
    }

    ImGui::Separator();
    ImGui::InputText("Recording File", debug.file_path.data(), debug.file_path.size());
    if (ImGui::Button("Save Recording")) {
        SaveRecordingToFile(debug, &debug.io_status);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Recording")) {
        LoadRecordingFromFile(debug, &debug.io_status);
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Text")) {
        ExportRecordingToTextFile(debug, graphics, &debug.io_status);
    }
    if (!debug.io_status.empty()) {
        ImGui::TextWrapped("%s", debug.io_status.c_str());
    }

    if (debug.playback_active && !debug.recorded_snapshots.empty()) {
        ClampPlaybackIndex(debug);
        int playback_index = static_cast<int>(debug.playback_index);
        const int max_index = static_cast<int>(debug.recorded_snapshots.size()) - 1;
        ImGui::Separator();
        if (ImGui::Button("|<")) {
            debug.playback_index = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("<")) {
            if (debug.playback_index > 0) {
                debug.playback_index -= 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(">")) {
            if (debug.playback_index + 1 < debug.recorded_snapshots.size()) {
                debug.playback_index += 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(">|")) {
            debug.playback_index = debug.recorded_snapshots.size() - 1;
        }
        if (ImGui::SliderInt("Playback Frame", &playback_index, 0, max_index)) {
            debug.playback_index = static_cast<std::size_t>(playback_index);
        }
        ImGui::Text("Frame %zu / %zu", debug.playback_index, debug.recorded_snapshots.size() - 1);
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawLevelControls(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.level_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(360.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Level", &debug.level_window_visible)) {
        ImGui::End();
        return;
    }

    if (debug.playback_active) {
        ImGui::BeginDisabled();
    }

    const auto apply_stage_fit_camera = [&state, &graphics]() {
        graphics.camera_mode = CameraMode::StageFit;
        graphics.play_cam.pos = GetStageCameraCenter(state.stage);
        graphics.camera.target = graphics.play_cam.pos;
        graphics.camera.zoom = GetStageFitCameraZoom(state.stage, graphics);
    };

    ImGui::SeparatorText("Quest Stages");
    QuestDefinition quest;
    bool quest_loaded = false;
    try {
        quest = LoadQuestDefinition(std::string(GetClassicQuestRootPath()) + "/quest.yaml");
        quest_loaded = true;
    } catch (const std::exception& e) {
        ImGui::TextColored(ImVec4(1.0F, 0.25F, 0.2F, 1.0F), "Quest load failed: %s", e.what());
    }

    if (quest_loaded) {
        ImGui::Text("Quest: %s", quest.title.c_str());
        const std::vector<std::string> stage_labels = BuildQuestStageLabels(quest);
        std::vector<const char*> quest_stage_names;
        quest_stage_names.reserve(stage_labels.size());
        for (const std::string& label : stage_labels) {
            quest_stage_names.push_back(label.c_str());
        }
        if (!quest_stage_names.empty()) {
            debug.quest_stage_index = std::clamp(
                debug.quest_stage_index,
                0,
                static_cast<int>(quest_stage_names.size()) - 1
            );
            ImGui::Combo(
                "Quest Stage",
                &debug.quest_stage_index,
                quest_stage_names.data(),
                static_cast<int>(quest_stage_names.size())
            );
            ImGui::Checkbox("Preserve player state", &debug.quest_stage_preserve_player_state);
            ImGui::Checkbox("Use quest RNG seed", &debug.quest_stage_use_seed);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0F);
            ImGui::InputInt("Seed", &debug.quest_stage_seed);
            debug.quest_stage_seed = std::max(0, debug.quest_stage_seed);

            const auto current_seed = [&debug]() -> std::optional<std::uint32_t> {
                if (!debug.quest_stage_use_seed) {
                    return std::nullopt;
                }
                return static_cast<std::uint32_t>(debug.quest_stage_seed);
            };

            if (ImGui::Button("Load / Reroll Selected")) {
                LoadClassicQuestStage(
                    state,
                    graphics,
                    quest,
                    debug.quest_stage_index,
                    debug.quest_stage_preserve_player_state,
                    current_seed()
                );
                debug.quest_stage_status = debug.quest_stage_use_seed
                                             ? "Generated with seed " + std::to_string(debug.quest_stage_seed)
                                             : "Generated with random RNG state";
            }
            const std::optional<int> current_quest_stage = GetClassicQuestStageIndex(quest, state);
            ImGui::SameLine();
            if (!current_quest_stage.has_value()) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Reroll Current")) {
                LoadClassicQuestStage(
                    state,
                    graphics,
                    quest,
                    current_quest_stage.value_or(debug.quest_stage_index),
                    debug.quest_stage_preserve_player_state,
                    current_seed()
                );
                debug.quest_stage_status = debug.quest_stage_use_seed
                                             ? "Rerolled current with seed " + std::to_string(debug.quest_stage_seed)
                                             : "Rerolled current with random RNG state";
            }
            if (!current_quest_stage.has_value()) {
                ImGui::EndDisabled();
            }
            if (debug.quest_stage_use_seed) {
                ImGui::SameLine();
                if (ImGui::Button("Next Seed + Reroll")) {
                    debug.quest_stage_seed += 1;
                    LoadClassicQuestStage(
                        state,
                        graphics,
                        quest,
                        current_quest_stage.value_or(debug.quest_stage_index),
                        debug.quest_stage_preserve_player_state,
                        static_cast<std::uint32_t>(debug.quest_stage_seed)
                    );
                    debug.quest_stage_status = "Rerolled with seed " + std::to_string(debug.quest_stage_seed);
                }
            }
            if (current_quest_stage.has_value()) {
                ImGui::Text("Active Quest Stage: %s", stage_labels[static_cast<std::size_t>(*current_quest_stage)].c_str());
            } else {
                ImGui::TextUnformatted("Active Quest Stage: <none>");
            }
            if (!debug.quest_stage_status.empty()) {
                ImGui::TextWrapped("%s", debug.quest_stage_status.c_str());
            }
        }
    }
    ImGui::TextUnformatted("Quest and room YAML reload when a quest stage is generated.");
    ImGui::Checkbox("Show stagegen annotation overlay", &state.debug_overlay.show_stagegen_annotations);
    ImGui::Text("Stagegen annotations: %zu", state.stage.stagegen_annotations.size());
    if (ImGui::CollapsingHeader("Stagegen Annotation List")) {
        for (const StageGenAnnotation& annotation : state.stage.stagegen_annotations) {
            ImGui::BulletText("(%.0f, %.0f) %s", annotation.world_pos.x, annotation.world_pos.y, annotation.text.c_str());
        }
    }

    ImGui::SeparatorText("Debug Presets");
    const DebugLevelKind previous_level_kind = state.debug_level.kind;
    int level_kind = std::clamp(static_cast<int>(state.debug_level.kind), 0, kDebugLevelKindCount - 1);
    const char* level_names[kDebugLevelKindCount] = {};
    for (int i = 0; i < kDebugLevelKindCount; ++i) {
        level_names[i] = GetDebugLevelKindName(static_cast<DebugLevelKind>(i));
    }
    ImGui::Combo("Preset", &level_kind, level_names, kDebugLevelKindCount);
    level_kind = std::clamp(level_kind, 0, kDebugLevelKindCount - 1);
    state.debug_level.kind = static_cast<DebugLevelKind>(level_kind);
    if (previous_level_kind != state.debug_level.kind &&
        (state.debug_level.kind == DebugLevelKind::TrapDoorTest ||
         state.debug_level.kind == DebugLevelKind::CrusherTrapTest)) {
        graphics.camera_mode = CameraMode::Follow;
    }
    if (previous_level_kind != state.debug_level.kind &&
        (state.debug_level.kind == DebugLevelKind::BowlingTest ||
         state.debug_level.kind == DebugLevelKind::OpposingBodySmack ||
         state.debug_level.kind == DebugLevelKind::BoulderTest ||
         state.debug_level.kind == DebugLevelKind::MovingPlatformTest ||
         state.debug_level.kind == DebugLevelKind::AudioTest ||
         state.debug_level.kind == DebugLevelKind::ParachuteTest ||
         state.debug_level.kind == DebugLevelKind::SacAltarTest ||
         state.debug_level.kind == DebugLevelKind::SpikeTest)) {
        apply_stage_fit_camera();
    }
    ImGui::Text("Active: %s", DebugLevelKindToString(state.debug_level.kind));
    if (ImGui::Button("Give Players Gloves")) {
        for (Entity& entity : state.entity_manager.entities) {
            if (!entity.active || !IsPlayerLikeEntityType(entity.type_)) {
                continue;
            }
            SetEffect(entity, EffectId::Gloves, true);
        }
    }

    if (state.debug_level.kind == DebugLevelKind::HangTest) {
        HangTestLevelConfig& hang_test = state.debug_level.hang_test;
        ImGui::SliderInt("Drop", &hang_test.drop_tiles, 0, 64);
    } else if (state.debug_level.kind == DebugLevelKind::BorderTest) {
        ImGui::TextUnformatted("Use the Border window to edit side tiles, wrap, and void death.");
    } else if (state.debug_level.kind == DebugLevelKind::MazeDoorTest) {
        int maze_room = static_cast<int>(state.debug_level.maze_door_test.room);
        const char* maze_room_names[] = {"RoomA", "RoomB", "RoomC"};
        ImGui::Combo("Maze Room", &maze_room, maze_room_names, IM_ARRAYSIZE(maze_room_names));
        state.debug_level.maze_door_test.room = static_cast<MazeDoorTestRoom>(maze_room);
    } else if (state.debug_level.kind == DebugLevelKind::CrusherTrapTest) {
        ImGui::SliderInt(
            "Squisher Sensor Tiles",
            &state.debug_level.crusher_trap_test.squisher_sensor_tiles,
            0,
            256
        );
        ImGui::TextUnformatted("0 = full stage reach");
        ImGui::SliderInt(
            "Stress Squisher Count",
            &state.debug_level.crusher_trap_test.stress_squisher_count,
            0,
            static_cast<int>(EntityManager::kMaxNumEntities - 8)
        );
    }

    if (ImGui::Button("Regenerate")) {
        InitDebugLevel(state);
        if (state.debug_level.kind == DebugLevelKind::BorderTest) {
            ApplyBorderTestWrapConfig(state, graphics);
        } else if (
            state.debug_level.kind == DebugLevelKind::BowlingTest ||
            state.debug_level.kind == DebugLevelKind::OpposingBodySmack ||
            state.debug_level.kind == DebugLevelKind::BoulderTest ||
            state.debug_level.kind == DebugLevelKind::MovingPlatformTest ||
            state.debug_level.kind == DebugLevelKind::AudioTest ||
            state.debug_level.kind == DebugLevelKind::ParachuteTest ||
            state.debug_level.kind == DebugLevelKind::SacAltarTest ||
            state.debug_level.kind == DebugLevelKind::SpikeTest) {
            apply_stage_fit_camera();
        } else if (state.debug_level.kind == DebugLevelKind::TrapDoorTest ||
                   state.debug_level.kind == DebugLevelKind::CrusherTrapTest) {
            graphics.camera_mode = CameraMode::Follow;
        }
        graphics.ResetTileVariations();
        InvalidateStageLighting(state);
        InvalidateStageAcoustics(state);
    }

    if (debug.playback_active) {
        ImGui::EndDisabled();
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawBorderControls(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.border_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Border", &debug.border_window_visible)) {
        ImGui::End();
        return;
    }

    if (debug.playback_active) {
        ImGui::BeginDisabled();
    }

    bool border_changed = false;
    bool wrap_settings_changed = false;

    if (state.debug_level.kind == DebugLevelKind::BorderTest) {
        BorderTestLevelConfig& border_test = state.debug_level.border_test;
        border_changed |= DrawTileCombo("Left Tile", border_test.left_tile);
        border_changed |= DrawTileCombo("Right Tile", border_test.right_tile);
        border_changed |= DrawTileCombo("Top Tile", border_test.top_tile);
        border_changed |= DrawTileCombo("Bottom Tile", border_test.bottom_tile);
        const bool wrap_x_changed = ImGui::Checkbox("Wrap X", &border_test.wrap_x);
        const bool wrap_y_changed = ImGui::Checkbox("Wrap Y", &border_test.wrap_y);
        wrap_settings_changed |= wrap_x_changed;
        wrap_settings_changed |= wrap_y_changed;
        if (wrap_x_changed || wrap_y_changed) {
            border_test.camera_clamp_enabled = !(border_test.wrap_x || border_test.wrap_y);
        }

        int wrap_padding_tiles = border_test.wrap_padding_tiles;
        if (ImGui::InputInt("Wrap Padding Tiles", &wrap_padding_tiles)) {
            border_test.wrap_padding_tiles = std::max(0, wrap_padding_tiles);
            wrap_settings_changed = true;
        }

        wrap_settings_changed |= ImGui::Checkbox("Camera Clamp", &border_test.camera_clamp_enabled);

        bool has_void_death_y = border_test.void_death_y.has_value();
        if (ImGui::Checkbox("Void Death Enabled", &has_void_death_y)) {
            border_test.void_death_y = has_void_death_y
                                           ? std::optional<int>(DefaultVoidDeathYForStage(state.stage))
                                           : std::nullopt;
            border_changed = true;
        }
        if (border_test.void_death_y.has_value()) {
            int void_death_y = *border_test.void_death_y;
            if (ImGui::InputInt("Void Death Y", &void_death_y)) {
                border_test.void_death_y = void_death_y;
                border_changed = true;
            }
        }

        if (border_changed) {
            CopyBorderConfigToStage(border_test, state.stage);
        }
        if (wrap_settings_changed) {
            ApplyBorderTestWrapConfig(state, graphics);
        }
    } else {
        border_changed |= DrawTileCombo("Left Tile", state.stage.border.left.tile);
        border_changed |= DrawTileCombo("Right Tile", state.stage.border.right.tile);
        border_changed |= DrawTileCombo("Top Tile", state.stage.border.top.tile);
        border_changed |= DrawTileCombo("Bottom Tile", state.stage.border.bottom.tile);

        bool wrap_x = state.stage.border.wrap_x;
        bool wrap_y = state.stage.border.wrap_y;
        int wrap_padding_tiles = static_cast<int>(state.stage.wrap_padding_tiles);
        bool camera_clamp_enabled = state.stage.camera_clamp_enabled;
        const bool wrap_x_changed = ImGui::Checkbox("Wrap X", &wrap_x);
        const bool wrap_y_changed = ImGui::Checkbox("Wrap Y", &wrap_y);
        wrap_settings_changed |= wrap_x_changed;
        wrap_settings_changed |= wrap_y_changed;
        if (wrap_x_changed || wrap_y_changed) {
            camera_clamp_enabled = !(wrap_x || wrap_y);
        }
        if (ImGui::InputInt("Wrap Padding Tiles", &wrap_padding_tiles)) {
            wrap_padding_tiles = std::max(0, wrap_padding_tiles);
            wrap_settings_changed = true;
        }
        wrap_settings_changed |= ImGui::Checkbox("Camera Clamp", &camera_clamp_enabled);

        bool has_void_death_y = state.stage.border.void_death_y.has_value();
        if (ImGui::Checkbox("Void Death Enabled", &has_void_death_y)) {
            state.stage.border.void_death_y = has_void_death_y
                                                  ? std::optional<int>(DefaultVoidDeathYForStage(state.stage))
                                                  : std::nullopt;
            border_changed = true;
        }
        if (state.stage.border.void_death_y.has_value()) {
            int void_death_y = *state.stage.border.void_death_y;
            if (ImGui::InputInt("Void Death Y", &void_death_y)) {
                state.stage.border.void_death_y = void_death_y;
                border_changed = true;
            }
        }

        if (wrap_settings_changed) {
            ApplyToroidalWrapSettings(
                state,
                graphics,
                wrap_x,
                wrap_y,
                static_cast<unsigned int>(wrap_padding_tiles),
                camera_clamp_enabled
            );
        }
    }

    ImGui::Separator();
    ImGui::Text(
        "Current: L=%s R=%s T=%s B=%s",
        TileToString(state.stage.border.left.tile),
        TileToString(state.stage.border.right.tile),
        TileToString(state.stage.border.top.tile),
        TileToString(state.stage.border.bottom.tile)
    );
    ImGui::Text(
        "Wrap: X=%s Y=%s Padding=%u",
        state.stage.border.wrap_x ? "on" : "off",
        state.stage.border.wrap_y ? "on" : "off",
        state.stage.wrap_padding_tiles
    );
    ImGui::Text("Camera Clamp: %s", state.stage.camera_clamp_enabled ? "on" : "off");
    ImGui::Text(
        "Void Death Y: %s",
        state.stage.border.void_death_y.has_value()
            ? std::to_string(*state.stage.border.void_death_y).c_str()
            : "off"
    );

    if (border_changed) {
        graphics.ResetTileVariations();
        InvalidateStageLighting(state);
        InvalidateStageAcoustics(state);
    }

    if (debug.playback_active) {
        ImGui::EndDisabled();
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

} // namespace splonks::debug_playback_internal

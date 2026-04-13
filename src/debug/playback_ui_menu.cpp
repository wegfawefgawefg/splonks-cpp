#include "debug/playback_internal.hpp"

#include "imgui_layer.hpp"
#include "stage_init.hpp"
#include "render/terrain_lighting.hpp"

#include <imgui.h>

#include <algorithm>

namespace splonks::debug_playback_internal {

namespace {

constexpr float kMinTimeScale = 0.01F;
constexpr float kMaxTimeScale = 2.0F;
constexpr int kMinSnapshots = 1;
constexpr int kMaxSnapshots = 20000;

int DefaultVoidDeathYForStage(const Stage& stage) {
    return static_cast<int>(stage.GetHeight() + stage.GetRoomDims().y);
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
    stage.border.wrap_x = config.wrap_x;
    stage.border.wrap_y = config.wrap_y;
    stage.border.void_death_y = config.void_death_y;
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
    ImGui::Checkbox("Entity Annotations", &debug.entity_annotations_visible);
    ImGui::Checkbox("UI Settings", &debug.ui_settings_window_visible);
    ImGui::Checkbox("Post FX Settings", &debug.post_fx_settings_window_visible);
    ImGui::Checkbox("Lighting Settings", &debug.lighting_settings_window_visible);
    ImGui::Checkbox("Graphics Settings", &debug.graphics_settings_window_visible);
    ImGui::Separator();
    ImGui::TextUnformatted("Shortcuts");
    ImGui::TextUnformatted("F1: Toggle all ImGui");
    ImGui::TextUnformatted("F2: Toggle debug menu");
    ImGui::TextUnformatted("Collision boxes moved to Entity Annotations.");
    ImGui::Separator();
    ImGui::Text("Playback Active: %s", debug.playback_active ? "true" : "false");
    ImGui::Text("Selected Entity: %zu", debug.selected_entity_id);
    ImGui::Text("Entity P/C boxes: %s", state.show_entity_collision_boxes ? "on" : "off");

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawSimulationControls(DebugPlayback& debug, State& state, Graphics& graphics) {
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

    int level_kind = static_cast<int>(state.debug_level.kind);
    const char* level_names[] = {"Cave1", "HangTest", "StompTest", "BorderTest"};
    ImGui::Combo("Preset", &level_kind, level_names, IM_ARRAYSIZE(level_names));
    state.debug_level.kind = static_cast<DebugLevelKind>(level_kind);
    ImGui::Text("Active: %s", DebugLevelKindToString(state.debug_level.kind));
    if (ImGui::Button("Give Players Gloves")) {
        for (Entity& entity : state.entity_manager.entities) {
            if (!entity.active || entity.type_ != EntityType::Player) {
                continue;
            }
            SetPassiveItem(entity, EntityPassiveItem::Gloves, true);
        }
    }

    if (state.debug_level.kind == DebugLevelKind::HangTest) {
        HangTestLevelConfig& hang_test = state.debug_level.hang_test;
        ImGui::SliderInt("Stage Height", &hang_test.stage_height_tiles, 16, 512);
        const int cutout_drop_max = std::max(2, hang_test.stage_height_tiles - 8);
        const int cutout_height_max =
            std::max(1, hang_test.stage_height_tiles - 7 - hang_test.cutout_drop_tiles);

        ImGui::SliderInt("Cutout Drop", &hang_test.cutout_drop_tiles, 2, cutout_drop_max);
        ImGui::SliderInt(
            "Cutout Height",
            &hang_test.cutout_height_tiles,
            1,
            std::min(8, cutout_height_max)
        );
    } else if (state.debug_level.kind == DebugLevelKind::BorderTest) {
        ImGui::TextUnformatted("Use the Border window to edit side tiles, wrap, and void death.");
    }

    if (ImGui::Button("Regenerate")) {
        InitDebugLevel(state);
        graphics.ResetTileVariations();
        InvalidateTerrainLightingCache(state);
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

    bool changed = false;
    if (state.debug_level.kind == DebugLevelKind::BorderTest) {
        BorderTestLevelConfig& border_test = state.debug_level.border_test;
        changed |= DrawTileCombo("Left Tile", border_test.left_tile);
        changed |= DrawTileCombo("Right Tile", border_test.right_tile);
        changed |= DrawTileCombo("Top Tile", border_test.top_tile);
        changed |= DrawTileCombo("Bottom Tile", border_test.bottom_tile);
        changed |= ImGui::Checkbox("Wrap X", &border_test.wrap_x);
        changed |= ImGui::Checkbox("Wrap Y", &border_test.wrap_y);
        bool has_void_death_y = border_test.void_death_y.has_value();
        if (ImGui::Checkbox("Void Death Enabled", &has_void_death_y)) {
            border_test.void_death_y = has_void_death_y
                                           ? std::optional<int>(DefaultVoidDeathYForStage(state.stage))
                                           : std::nullopt;
            changed = true;
        }
        if (border_test.void_death_y.has_value()) {
            int void_death_y = *border_test.void_death_y;
            if (ImGui::InputInt("Void Death Y", &void_death_y)) {
                border_test.void_death_y = void_death_y;
                changed = true;
            }
        }

        if (changed) {
            CopyBorderConfigToStage(border_test, state.stage);
        }
    } else {
        changed |= DrawTileCombo("Left Tile", state.stage.border.left.tile);
        changed |= DrawTileCombo("Right Tile", state.stage.border.right.tile);
        changed |= DrawTileCombo("Top Tile", state.stage.border.top.tile);
        changed |= DrawTileCombo("Bottom Tile", state.stage.border.bottom.tile);
        changed |= ImGui::Checkbox("Wrap X", &state.stage.border.wrap_x);
        changed |= ImGui::Checkbox("Wrap Y", &state.stage.border.wrap_y);
        bool has_void_death_y = state.stage.border.void_death_y.has_value();
        if (ImGui::Checkbox("Void Death Enabled", &has_void_death_y)) {
            state.stage.border.void_death_y = has_void_death_y
                                                  ? std::optional<int>(DefaultVoidDeathYForStage(state.stage))
                                                  : std::nullopt;
            changed = true;
        }
        if (state.stage.border.void_death_y.has_value()) {
            int void_death_y = *state.stage.border.void_death_y;
            if (ImGui::InputInt("Void Death Y", &void_death_y)) {
                state.stage.border.void_death_y = void_death_y;
                changed = true;
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Current: L=%s R=%s T=%s B=%s",
                TileToString(state.stage.border.left.tile),
                TileToString(state.stage.border.right.tile),
                TileToString(state.stage.border.top.tile),
                TileToString(state.stage.border.bottom.tile));
    ImGui::Text("Wrap: X=%s Y=%s",
                state.stage.border.wrap_x ? "on" : "off",
                state.stage.border.wrap_y ? "on" : "off");
    ImGui::Text(
        "Void Death Y: %s",
        state.stage.border.void_death_y.has_value()
            ? std::to_string(*state.stage.border.void_death_y).c_str()
            : "off"
    );

    if (changed) {
        graphics.ResetTileVariations();
        InvalidateTerrainLightingCache(state);
    }

    if (debug.playback_active) {
        ImGui::EndDisabled();
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

} // namespace splonks::debug_playback_internal

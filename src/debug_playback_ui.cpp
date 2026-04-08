#include "debug_playback_internal.hpp"

#include "frame_data.hpp"
#include "imgui_layer.hpp"
#include "settings.hpp"
#include "stage_init.hpp"

#include <imgui.h>

#include <algorithm>

namespace splonks::debug_playback_internal {

namespace {

constexpr float kMinTimeScale = 0.01F;
constexpr float kMaxTimeScale = 2.0F;
constexpr int kMinSnapshots = 1;
constexpr int kMaxSnapshots = 20000;

const char* ToolKindToString(ToolKind tool_kind) {
    switch (tool_kind) {
    case ToolKind::ThrowPot:
        return "ThrowPot";
    case ToolKind::ThrowBomb:
        return "ThrowBomb";
    case ToolKind::ThrowRope:
        return "ThrowRope";
    }

    return "Unknown";
}

bool SyncDebugUiSettings(DebugPlayback& debug, State& state) {
    bool changed = false;

    if (state.settings.debug_ui.menu_visible != debug.ui_visible) {
        state.settings.debug_ui.menu_visible = debug.ui_visible;
        changed = true;
    }
    if (state.settings.debug_ui.playback_visible != debug.playback_window_visible) {
        state.settings.debug_ui.playback_visible = debug.playback_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.level_visible != debug.level_window_visible) {
        state.settings.debug_ui.level_visible = debug.level_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.entities_visible != debug.entity_inspector_visible) {
        state.settings.debug_ui.entities_visible = debug.entity_inspector_visible;
        changed = true;
    }
    if (state.settings.debug_ui.entity_annotations_visible != debug.entity_annotations_visible) {
        state.settings.debug_ui.entity_annotations_visible = debug.entity_annotations_visible;
        changed = true;
    }
    if (state.settings.debug_ui.ui_settings_visible != debug.ui_settings_window_visible) {
        state.settings.debug_ui.ui_settings_visible = debug.ui_settings_window_visible;
        changed = true;
    }

    if (changed) {
        SaveSettings(state.settings);
    }

    return changed;
}

} // namespace

void DrawDebugMenu(DebugPlayback& debug, State& state) {
    if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
        debug.ui_visible = !debug.ui_visible;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        debug.playback_window_visible = !debug.playback_window_visible;
    }

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
    ImGui::Checkbox("Entities", &debug.entity_inspector_visible);
    ImGui::Checkbox("Entity Annotations", &debug.entity_annotations_visible);
    ImGui::Checkbox("UI Settings", &debug.ui_settings_window_visible);
    ImGui::Separator();
    ImGui::TextUnformatted("Shortcuts");
    ImGui::TextUnformatted("F1: Toggle this menu");
    ImGui::TextUnformatted("F2: Toggle playback window");
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
    const char* level_names[] = {"Test1", "HangTest"};
    ImGui::Combo("Preset", &level_kind, level_names, IM_ARRAYSIZE(level_names));
    state.debug_level.kind = static_cast<DebugLevelKind>(level_kind);
    ImGui::Text("Active: %s", DebugLevelKindToString(state.debug_level.kind));
    ImGui::Checkbox("Players Have Gloves", &state.debug_level.player_has_gloves);

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
    }

    if (ImGui::Button("Regenerate")) {
        InitDebugLevel(state);
        graphics.ResetTileVariations();
    }

    if (debug.playback_active) {
        ImGui::EndDisabled();
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawEntityAnnotations(DebugPlayback& debug, State& state) {
    if (!debug.entity_annotations_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Entity Annotations", &debug.entity_annotations_visible)) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Show Entity P/C Boxes", &state.show_entity_collision_boxes);
    ImGui::TextUnformatted("PBox/CBox overlay uses render debug colors.");

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawUiSettingsWindow(DebugPlayback& debug, State& state) {
    if (!debug.ui_settings_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(860.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: UI Settings", &debug.ui_settings_window_visible)) {
        ImGui::End();
        return;
    }

    bool changed = false;
    changed |= ImGui::SliderFloat(
        "Icon Scale",
        &state.settings.ui.icon_scale,
        0.25F,
        1.50F,
        "%.2fx"
    );
    changed |= ImGui::SliderFloat(
        "Status Icon Scale",
        &state.settings.ui.status_icon_scale,
        0.25F,
        1.50F,
        "%.2fx"
    );
    changed |= ImGui::SliderFloat(
        "Tool Slot Scale",
        &state.settings.ui.tool_slot_scale,
        0.25F,
        1.50F,
        "%.2fx"
    );
    changed |= ImGui::SliderFloat(
        "Tool Icon Scale",
        &state.settings.ui.tool_icon_scale,
        0.25F,
        1.50F,
        "%.2fx"
    );

    if (changed) {
        SaveSettings(state.settings);
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawEntityInspector(DebugPlayback& debug, State& state, const Graphics& graphics) {
    if (!debug.entity_inspector_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(12.0F, 300.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Entities", &debug.entity_inspector_visible)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginListBox("Entities", ImVec2(260.0F, 220.0F))) {
        for (std::size_t i = 0; i < state.entity_manager.entities.size(); ++i) {
            const Entity& entity = state.entity_manager.entities[i];
            if (!entity.active) {
                continue;
            }

            char label[128];
            std::snprintf(
                label,
                sizeof(label),
                "%zu: %s##entity_%zu",
                i,
                EntityTypeToString(entity.type_),
                i
            );
            const bool selected = debug.selected_entity_id == i;
            if (ImGui::Selectable(label, selected)) {
                debug.selected_entity_id = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }

    if (debug.selected_entity_id >= state.entity_manager.entities.size()) {
        debug.selected_entity_id = 0;
    }

    const Entity* selected_entity = nullptr;
    if (!state.entity_manager.entities.empty()) {
        const Entity& entity = state.entity_manager.entities[debug.selected_entity_id];
        if (entity.active) {
            selected_entity = &entity;
        }
    }

    if (selected_entity == nullptr) {
        ImGui::TextUnformatted("No active entity selected.");
        ImGui::End();
        return;
    }

    const Entity& entity = *selected_entity;
    const AABB aabb = entity.GetAABB();
    ImGui::Separator();
    ImGui::Text("Type: %s", EntityTypeToString(entity.type_));
    ImGui::Text(
        "Controlled: %s",
        state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid
            ? "true"
            : "false"
    );
    if (ImGui::Button("Control Selected")) {
        state.controlled_entity_vid = entity.vid;
    }
    if (state.player_vid.has_value()) {
        ImGui::SameLine();
        if (ImGui::Button("Control Player")) {
            state.controlled_entity_vid = *state.player_vid;
        }
    }
    ImGui::Text("Display: %s", DisplayStateToString(entity.display_state));
    ImGui::Text("Super: %s", SuperStateToString(entity.super_state));
    ImGui::Text("Facing: %s", LeftOrRightToString(entity.facing));
    ImGui::Text("Grounded: %s", entity.grounded ? "true" : "false");
    ImGui::Text("Pos: (%.2f, %.2f)", entity.pos.x, entity.pos.y);
    ImGui::Text("Vel: (%.2f, %.2f)", entity.vel.x, entity.vel.y);
    ImGui::Text("Acc: (%.2f, %.2f)", entity.acc.x, entity.acc.y);
    ImGui::Text("Size: (%.2f, %.2f)", entity.size.x, entity.size.y);
    ImGui::Text("AABB TL: (%.2f, %.2f)", aabb.tl.x, aabb.tl.y);
    ImGui::Text("AABB BR: (%.2f, %.2f)", aabb.br.x, aabb.br.y);
    ImGui::Text("Coyote: %u", entity.coyote_time);
    ImGui::Text("Health: %u", entity.health);
    ImGui::Text("Money: %u", entity.money);
    ImGui::Separator();
    ImGui::TextUnformatted("Tools");
    if (debug.playback_active) {
        ImGui::TextDisabled("Tool editing disabled during playback.");
    } else {
        const char* tool_kind_names[] = {"ThrowPot", "ThrowBomb", "ThrowRope"};
        for (std::size_t slot_index = 0; slot_index < kToolSlotCount; ++slot_index) {
            ToolSlot preview_slot{};
            preview_slot.kind = slot_index == 0 ? ToolKind::ThrowBomb : ToolKind::ThrowRope;
            ToolSlot* slot = state.FindToolSlotMut(entity.vid, slot_index);
            if (slot == nullptr) {
                slot = &preview_slot;
            }
            ImGui::PushID(static_cast<int>(slot_index));
            ImGui::SeparatorText(slot_index == 0 ? "Tool Slot 1" : "Tool Slot 2");
            bool active = slot->active;
            if (ImGui::Checkbox("Active", &active)) {
                ToolSlot& owned_slot = state.EnsureToolSlot(entity.vid, slot_index);
                owned_slot = *slot;
                owned_slot.active = active;
                slot = &owned_slot;
            }

            int kind_index = static_cast<int>(slot->kind);
            if (ImGui::Combo("Kind", &kind_index, tool_kind_names, IM_ARRAYSIZE(tool_kind_names))) {
                ToolSlot& owned_slot = state.EnsureToolSlot(entity.vid, slot_index);
                owned_slot = *slot;
                owned_slot.kind = static_cast<ToolKind>(kind_index);
                slot = &owned_slot;
            }

            int count = static_cast<int>(slot->count);
            int cooldown = static_cast<int>(slot->cooldown);
            ImGui::SetNextItemWidth(120.0F);
            if (ImGui::InputInt("Count", &count)) {
                ToolSlot& owned_slot = state.EnsureToolSlot(entity.vid, slot_index);
                owned_slot = *slot;
                owned_slot.count = static_cast<std::uint16_t>(std::clamp(count, 0, 65535));
                slot = &owned_slot;
            }
            ImGui::SetNextItemWidth(120.0F);
            if (ImGui::InputInt("Cooldown", &cooldown)) {
                ToolSlot& owned_slot = state.EnsureToolSlot(entity.vid, slot_index);
                owned_slot = *slot;
                owned_slot.cooldown = static_cast<std::uint16_t>(std::clamp(cooldown, 0, 65535));
            }
            ImGui::PopID();
        }
    }
    ImGui::Text("Climbing: %s", entity.climbing ? "true" : "false");
    ImGui::Text("Holding: %s", entity.holding ? "true" : "false");
    ImGui::Text("Horiz Controlled: %s", entity.IsHorizontallyControlled() ? "true" : "false");

    if (entity.frame_data_animator.HasAnimation()) {
        const FrameDataAnimation* animation =
            graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
        if (animation != nullptr) {
            ImGui::Text("Anim: %s", animation->name.c_str());
            ImGui::Text(
                "Anim Frame: %zu / %zu",
                entity.frame_data_animator.current_frame,
                animation->frame_indices.empty() ? 0 : animation->frame_indices.size() - 1
            );
            const FrameData* frame_data = graphics.frame_data_db.FindFrame(
                entity.frame_data_animator.animation_id,
                entity.frame_data_animator.current_frame
            );
            if (frame_data != nullptr) {
                ImGui::Text("Frame Duration: %d", frame_data->duration);
                ImGui::Text(
                    "Sample: (%d, %d, %d, %d)",
                    frame_data->sample_rect.x,
                    frame_data->sample_rect.y,
                    frame_data->sample_rect.w,
                    frame_data->sample_rect.h
                );
                ImGui::Text(
                    "Draw Offset: (%d, %d)",
                    frame_data->draw_offset.x,
                    frame_data->draw_offset.y
                );
                ImGui::Text(
                    "PBox: (%d, %d, %d, %d)",
                    frame_data->pbox.x,
                    frame_data->pbox.y,
                    frame_data->pbox.w,
                    frame_data->pbox.h
                );
                ImGui::Text(
                    "CBox: (%d, %d, %d, %d)",
                    frame_data->cbox.x,
                    frame_data->cbox.y,
                    frame_data->cbox.w,
                    frame_data->cbox.h
                );
            }
        }
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

} // namespace splonks::debug_playback_internal

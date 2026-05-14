#include "debug/playback_internal.hpp"

#include "audio_acoustics.hpp"
#include "audio_emitters.hpp"
#include "ents/common/common.hpp"
#include "settings.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"

#include <imgui.h>

#include <cstdio>

namespace splonks::debug_playback_internal {

namespace {

bool IsPeerMechanicsTuningDisabled(const State& state) {
    return state.net_session.role == network::NetRole::Peer;
}

bool IsPeerAdminControlDisabled(const State& state) {
    return state.net_session.role == network::NetRole::Peer;
}

const char* CameraModeToString(CameraMode mode) {
    switch (mode) {
    case CameraMode::Follow:
        return "Follow";
    case CameraMode::StageFit:
        return "StageFit";
    }
    return "Unknown";
}

const char* FormatBytes(std::size_t bytes, char* buffer, std::size_t buffer_size) {
    constexpr const char* kUnits[] = {"B", "KiB", "MiB", "GiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit_index = 0;
    while (value >= 1024.0 && unit_index < 3) {
        value /= 1024.0;
        ++unit_index;
    }
    std::snprintf(buffer, buffer_size, "%.2f %s", value, kUnits[unit_index]);
    return buffer;
}

void DrawTimingRow(const char* label, double smoothed_ms, double raw_ms, double peak_ms, double budget_ms) {
    const double smoothed_percent = budget_ms > 0.0 ? (smoothed_ms / budget_ms) * 100.0 : 0.0;
    const double peak_percent = budget_ms > 0.0 ? (peak_ms / budget_ms) * 100.0 : 0.0;
    ImGui::Text("%s: %.3f ms avg (raw %.3f)", label, smoothed_ms, raw_ms);
    ImGui::SameLine(320.0F);
    ImGui::Text("peak %.3f ms (%.1f%%)", peak_ms, peak_percent);
    ImGui::SameLine(560.0F);
    ImGui::Text("avg %.1f%%", smoothed_percent);
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

} // namespace

void DrawDebugOverlayWindow(DebugPlayback& debug, State& state, Graphics&) {
    if (!debug.ent_annotations_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Overlay", &debug.ent_annotations_visible)) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Show Ent P/C Boxes", &state.debug_overlay.show_ent_collision_boxes);
    ImGui::Checkbox("Show Ent IDs", &state.debug_overlay.show_ent_ids);
    ImGui::Checkbox("Show Ent Types", &state.debug_overlay.show_ent_types);
    ImGui::Checkbox("Show Ent Render Centers", &state.debug_overlay.show_ent_render_centers);
    ImGui::Checkbox("Show Void Death Line", &state.debug_overlay.show_void_death_line);
    ImGui::Checkbox("Show Chunk Boundaries", &state.debug_overlay.show_chunk_boundaries);
    ImGui::Checkbox("Show Chunk Coords", &state.debug_overlay.show_chunk_coords);
    ImGui::Checkbox("Show Tile Indexes", &state.debug_overlay.show_tile_indexes);
    ImGui::Checkbox("Show Tile Types", &state.debug_overlay.show_tile_types);
    ImGui::Checkbox("Show Tile Openness", &state.debug_overlay.show_tile_openness);
    ImGui::Checkbox("Show Fluid Amounts", &state.debug_overlay.show_fluid_amounts);
    ImGui::Checkbox("Show Fluid Gravity", &state.debug_overlay.show_fluid_gravity);
    ImGui::Checkbox("Show Lights", &state.debug_overlay.show_lights);
    ImGui::Checkbox("Show Area Boundaries", &state.debug_overlay.show_area_boundaries);
    ImGui::Checkbox("Show Area IDs", &state.debug_overlay.show_area_ids);
    ImGui::Checkbox("Show Area Types", &state.debug_overlay.show_area_types);
    ImGui::Checkbox("Show Audio Emitters", &state.debug_overlay.show_audio_emitters);
    ImGui::Checkbox("Show Debug Annotations", &state.debug_overlay.show_debug_annotations);
    ImGui::Checkbox("Show Stagegen Annotations", &state.debug_overlay.show_stagegen_annotations);
    ImGui::BeginDisabled(!state.debug_overlay.show_audio_emitters || !IsAudioOcclusionEnabled(state));
    ImGui::Checkbox("Show Audio Occlusion Paths", &state.debug_overlay.show_audio_occlusion_paths);
    ImGui::EndDisabled();
    ImGui::TextUnformatted("PBox/CBox overlay uses render debug colors.");

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawShakeBrushWindow(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.shake_brush_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 220.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Shake Brush", &debug.shake_brush_window_visible)) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Shake Brush", &state.debug_shake_brush.enabled);
    ImGui::Checkbox("Affect FG Tiles", &state.debug_shake_brush.affect_foreground_tiles);
    ImGui::SliderFloat(
        "FG Amount (px)",
        &state.debug_shake_brush.foreground_tile_amount,
        0.0F,
        8.0F,
        "%.2f px"
    );
    ImGui::Checkbox("Affect BG Tiles", &state.debug_shake_brush.affect_background_tiles);
    ImGui::SliderFloat(
        "BG Amount (px)",
        &state.debug_shake_brush.background_tile_amount,
        0.0F,
        8.0F,
        "%.2f px"
    );
    ImGui::Checkbox("Affect Ents", &state.debug_shake_brush.affect_ents);
    ImGui::SliderFloat(
        "Ent Amount (px)",
        &state.debug_shake_brush.ent_amount,
        0.0F,
        8.0F,
        "%.2f px"
    );
    ImGui::SliderFloat("Brush Radius (tiles)", &state.debug_shake_brush.radius_tiles, 0.0F, 12.0F, "%.2f");
    const Vec2 mouse_world = graphics.ScreenToWc(state.immediate_playing_inputs.mouse_pos);
    const IVec2 mouse_tile = graphics.ScreenToTileCoords(state.immediate_playing_inputs.mouse_pos);
    ImGui::Text("Mouse WC: (%.1f, %.1f)", mouse_world.x, mouse_world.y);
    ImGui::Text("Mouse Tile: (%d, %d)", mouse_tile.x, mouse_tile.y);
    ImGui::TextUnformatted("Hold left mouse in the world view to paint shake.");

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawAudioBrushWindow(DebugPlayback& debug, State& state, Audio& audio, Graphics& graphics) {
    if (!debug.audio_brush_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 460.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Audio Brush", &debug.audio_brush_window_visible)) {
        ImGui::End();
        return;
    }

    DebugAudioBrushState& brush = state.debug_audio_brush;
    const bool peer_admin_disabled = IsPeerAdminControlDisabled(state);
    if (peer_admin_disabled) {
        brush.enabled = false;
        brush.source_active = false;
        ImGui::BeginDisabled();
        ImGui::TextDisabled("Disabled on multiplayer peers. Sound brush is host-admin debug tooling.");
    }

    ImGui::Checkbox("Enable Audio Brush", &brush.enabled);
    ImGui::Checkbox("Show Openness Rays", &brush.show_openness_rays);
    ImGui::SameLine();
    ImGui::Checkbox("Show Occlusion Ray", &brush.show_occlusion_ray);
    const char* const selected_name = audio.GetAudioAssetNameCStr(brush.audio_asset_id);
    if (ImGui::BeginCombo("Loop Sound", selected_name)) {
        for (const AudioAsset& asset : audio.GetAssetDb().assets) {
            const bool selected = asset.id == brush.audio_asset_id;
            if (ImGui::Selectable(asset.name.c_str(), selected)) {
                brush.audio_asset_id = asset.id;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SliderFloat("Volume Scale", &brush.volume_scale, 0.0F, 2.0F, "%.2fx");

    const Vec2 mouse_world = graphics.ScreenToWc(state.immediate_playing_inputs.mouse_pos);
    ImGui::Text("Mouse WC: (%.1f, %.1f)", mouse_world.x, mouse_world.y);
    if (brush.source_active) {
        ImGui::Text(
            "Source WC: (%.1f, %.1f)",
            brush.source_world_pos.x,
            brush.source_world_pos.y
        );
    } else {
        ImGui::TextUnformatted("Source WC: <inactive>");
    }
    ImGui::Text(
        "Listener WC: (%.1f, %.1f)",
        GetAudioListenerWorldPos(state).x,
        GetAudioListenerWorldPos(state).y
    );
    if (brush.source_active) {
        const PositionalAudioAcoustics acoustics = ComputePositionalAudioAcoustics(
            state,
            GetAudioListenerWorldPos(state),
            brush.source_world_pos
        );
        ImGui::SeparatorText("Computed Acoustics");
        ImGui::Text("Source Open: %.2f", acoustics.source_openness);
        ImGui::Text("Listener Open: %.2f", acoustics.listener_openness);
        ImGui::Text("Direct Open: %.2f", acoustics.direct_open);
        ImGui::Text("Room Open: %.2f", acoustics.room_open);
        ImGui::Text("Occluded: %s", acoustics.occluded ? "yes" : "no");
        ImGui::Text("Listener Epsilon: %.1f px", state.settings.audio.acoustics_occlusion_listener_epsilon_px);
        ImGui::Text("Direct Gain: %.2f", acoustics.direct_gain);
        ImGui::Text("Direct Cutoff: %.0f Hz", acoustics.low_pass_cutoff_hz);
        ImGui::Text("Reverb Wet: %.2f", acoustics.reverb_wet);
        ImGui::Text("Reverb Feedback: %.2f", acoustics.reverb_feedback);
        ImGui::Text("Reverb Cutoff: %.0f Hz", acoustics.reverb_low_pass_cutoff_hz);
    }
    ImGui::Text("Openness Ray Length: %d tiles", kStageOpennessRayLengthTiles);

    if (ImGui::Button("Clear Source")) {
        brush.source_active = false;
    }

    ImGui::TextUnformatted("Hold left mouse in the world view to place or drag the loop source.");
    ImGui::TextUnformatted("Hold right mouse in the world view to clear it.");

    if (peer_admin_disabled) {
        ImGui::EndDisabled();
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawTileBrushWindow(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.tile_brush_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 520.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Tile Brush", &debug.tile_brush_window_visible)) {
        ImGui::End();
        return;
    }

    const bool peer_mutation_disabled = state.net_session.role == network::NetRole::Peer;
    if (peer_mutation_disabled) {
        debug.tile_brush_enabled = false;
        ImGui::BeginDisabled();
        ImGui::TextDisabled("Disabled on multiplayer peers until client admin commands are routed.");
    }

    ImGui::Checkbox("Enable Tile Brush", &debug.tile_brush_enabled);
    (void)DrawTileCombo("Paint Tile", debug.tile_brush_tile);
    ImGui::SliderInt("Rotation", &debug.tile_brush_rotation, 0, 3);
    ImGui::SliderInt("Brush Radius (tiles)", &debug.tile_brush_radius_tiles, 0, 16);
    const Vec2 mouse_world = graphics.ScreenToWc(state.immediate_playing_inputs.mouse_pos);
    const IVec2 mouse_tile = graphics.ScreenToTileCoords(state.immediate_playing_inputs.mouse_pos);
    ImGui::Text("Mouse WC: (%.1f, %.1f)", mouse_world.x, mouse_world.y);
    ImGui::Text("Mouse Tile: (%d, %d)", mouse_tile.x, mouse_tile.y);
    ImGui::TextUnformatted("Hold left mouse to paint foreground tiles. Hold right mouse to erase to Air.");

    if (peer_mutation_disabled) {
        ImGui::EndDisabled();
    }

    ImGui::End();
}

void DrawFluidBrushWindow(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.fluid_brush_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 700.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Fluid Brush", &debug.fluid_brush_window_visible)) {
        ImGui::End();
        return;
    }

    DebugFluidBrushState& brush = state.debug_fluid_brush;
    FluidSettings& fluid = state.settings.fluid;
    bool save_settings = false;
    const bool peer_tuning_disabled = IsPeerMechanicsTuningDisabled(state);
    if (peer_tuning_disabled) {
        ImGui::BeginDisabled();
        ImGui::TextDisabled("Disabled on multiplayer peers until mechanics settings are host-routed.");
    }
    ImGui::Checkbox("Enable Fluid Brush", &brush.enabled);
    save_settings |= ImGui::Checkbox("Run Fluid Simulation", &fluid.simulation_enabled);
    int brush_mode = static_cast<int>(brush.mode);
    ImGui::RadioButton("Water", &brush_mode, static_cast<int>(DebugFluidBrushState::Mode::Water));
    ImGui::SameLine();
    ImGui::RadioButton(
        "Gravity",
        &brush_mode,
        static_cast<int>(DebugFluidBrushState::Mode::PermanentGravity)
    );
    ImGui::SameLine();
    ImGui::RadioButton(
        "Temp Gravity",
        &brush_mode,
        static_cast<int>(DebugFluidBrushState::Mode::TemporaryGravity)
    );
    ImGui::RadioButton(
        "Set Global Gravity Direction From Stage Center",
        &brush_mode,
        static_cast<int>(DebugFluidBrushState::Mode::GlobalGravityDirection)
    );
    brush.mode = static_cast<DebugFluidBrushState::Mode>(brush_mode);
    save_settings |= ImGui::SliderInt("Sim Interval (frames)", &fluid.simulation_interval_frames, 1, 30);
    save_settings |=
        ImGui::SliderFloat("Transfer / Step (0-1)", &fluid.transfer_per_step, 0.0F, 1.0F, "%.3f");
    save_settings |= ImGui::SliderFloat("Global Gravity X", &fluid.gravity_x, -4.0F, 4.0F, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("0##FluidGravityX")) {
        fluid.gravity_x = 0.0F;
        save_settings = true;
    }
    save_settings |= ImGui::SliderFloat("Global Gravity Y", &fluid.gravity_y, -4.0F, 4.0F, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("0##FluidGravityY")) {
        fluid.gravity_y = 0.0F;
        save_settings = true;
    }
    if (ImGui::Button("Zero Gravity XY")) {
        fluid.gravity_x = 0.0F;
        fluid.gravity_y = 0.0F;
        save_settings = true;
    }
    ImGui::Text("Global Gravity: (%.2f, %.2f)", fluid.gravity_x, fluid.gravity_y);
    ImGui::SliderFloat("Brush Gravity X", &brush.paint_gravity_x, -4.0F, 4.0F, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("0##FluidPaintGravityX")) {
        brush.paint_gravity_x = 0.0F;
    }
    ImGui::SliderFloat("Brush Gravity Y", &brush.paint_gravity_y, -4.0F, 4.0F, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("0##FluidPaintGravityY")) {
        brush.paint_gravity_y = 0.0F;
    }
    if (ImGui::Button("Zero Brush Gravity XY")) {
        brush.paint_gravity_x = 0.0F;
        brush.paint_gravity_y = 0.0F;
    }
    ImGui::Text(
        "Painted Gravity: (%.2f, %.2f)",
        brush.paint_gravity_x,
        brush.paint_gravity_y
    );
    save_settings |= ImGui::SliderFloat("Pressure Strength", &fluid.pressure_strength, 0.0F, 4.0F, "%.2f");
    save_settings |= ImGui::SliderFloat("Velocity Damping", &fluid.velocity_damping, 0.0F, 1.0F, "%.2f");
    save_settings |= ImGui::SliderFloat("Temp Gravity Decay", &fluid.temp_gravity_decay, 0.0F, 1.0F, "%.2f");
    save_settings |= ImGui::Checkbox("Temporal Smoothing", &fluid.temporal_smoothing_enabled);
    save_settings |= ImGui::SliderFloat(
        "Temporal Response",
        &fluid.temporal_smoothing_response,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Render Cutoff (0-1)",
        &fluid.render_cutoff_amount,
        0.0F,
        1.0F,
        "%.4f"
    );
    save_settings |= ImGui::SliderFloat(
        "Global Water Alpha",
        &fluid.water_alpha,
        0.0F,
        1.0F,
        "%.2f"
    );
    ImGui::Checkbox("Show Flow Indicators", &brush.show_flow_indicators);
    save_settings |= ImGui::Checkbox("Water Uses Stage Lighting", &fluid.lighting_enabled);
    save_settings |= ImGui::SliderFloat(
        "Water Lighting Strength",
        &fluid.lighting_strength,
        0.0F,
        2.0F,
        "%.2f"
    );

    WaterEffectSettings& water_effect = state.settings.water_effect;
    ImGui::SeparatorText("Water Effect");
    if (ImGui::Button("Reset Water Effect")) {
        water_effect = WaterEffectSettings::New();
        save_settings = true;
    }
    save_settings |= ImGui::SliderFloat(
        "Effect Gravity Scale",
        &water_effect.gravity_scale,
        0.0F,
        2.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Damping X",
        &water_effect.velocity_damping_x,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Damping Y",
        &water_effect.velocity_damping_y,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Move Speed Scale",
        &water_effect.move_speed_scale,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Max Fall Speed",
        &water_effect.max_fall_speed,
        0.0F,
        12.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Buoyancy",
        &water_effect.buoyancy_strength,
        0.0F,
        4.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Fall Timer Rate",
        &water_effect.fall_timer_rate,
        0.0F,
        2.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Stomp Damage Scale",
        &water_effect.stomp_damage_scale,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Effect Swim Impulse",
        &water_effect.swim_impulse,
        0.0F,
        20.0F,
        "%.2f"
    );

    ImGui::SliderInt("Brush Radius (tiles)", &brush.radius_tiles, 0, 16);
    ImGui::Checkbox("Replace Non-Air Tiles", &brush.replace_solid_tiles);
    const Vec2 mouse_world = graphics.ScreenToWc(state.immediate_playing_inputs.mouse_pos);
    const IVec2 mouse_tile = graphics.ScreenToTileCoords(state.immediate_playing_inputs.mouse_pos);
    ImGui::Text("Mouse WC: (%.1f, %.1f)", mouse_world.x, mouse_world.y);
    ImGui::Text("Mouse Tile: (%d, %d)", mouse_tile.x, mouse_tile.y);
    ImGui::TextUnformatted("Hold left mouse to paint water. Hold right mouse to erase simulated fluid.");
    if (peer_tuning_disabled) {
        ImGui::EndDisabled();
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
    if (save_settings && !peer_tuning_disabled) {
        SaveSettings(state.settings);
    }
}

void DrawAudioSettingsWindow(DebugPlayback& debug, State& state) {
    if (!debug.audio_settings_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(860.0F, 460.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Audio Settings", &debug.audio_settings_window_visible)) {
        ImGui::End();
        return;
    }

    bool save_settings = false;
    const bool peer_admin_disabled = IsPeerAdminControlDisabled(state);
    if (peer_admin_disabled) {
        ImGui::BeginDisabled();
        ImGui::TextDisabled("Audio tuning is host-admin debug tooling in multiplayer.");
    }

    save_settings |= ImGui::SliderFloat(
        "Pan Half-Width (px)",
        &state.settings.audio.pan_half_width_px,
        16.0F,
        1024.0F,
        "%.0f px"
    );
    save_settings |= ImGui::Checkbox(
        "Acoustics Enabled",
        &state.settings.audio.acoustics_enabled
    );
    ImGui::Checkbox(
        "Occlusion Enabled",
        &state.audio_occlusion_enabled
    );
    save_settings |= ImGui::SliderFloat(
        "Occlusion Listener Epsilon",
        &state.settings.audio.acoustics_occlusion_listener_epsilon_px,
        0.0F,
        32.0F,
        "%.1f px"
    );
    save_settings |= ImGui::Checkbox(
        "Reverb Enabled",
        &state.settings.audio.acoustics_reverb_enabled
    );
    save_settings |= ImGui::SliderFloat(
        "Listener Room Weight",
        &state.settings.audio.acoustics_listener_room_weight,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Direct Min Cutoff",
        &state.settings.audio.acoustics_direct_min_cutoff_hz,
        100.0F,
        8000.0F,
        "%.0f Hz"
    );
    save_settings |= ImGui::SliderFloat(
        "Direct Max Cutoff",
        &state.settings.audio.acoustics_direct_max_cutoff_hz,
        1000.0F,
        20000.0F,
        "%.0f Hz"
    );
    save_settings |= ImGui::SliderFloat(
        "Occluded Cutoff",
        &state.settings.audio.acoustics_occluded_cutoff_hz,
        100.0F,
        8000.0F,
        "%.0f Hz"
    );
    save_settings |= ImGui::SliderFloat(
        "Occluded Direct Gain",
        &state.settings.audio.acoustics_occluded_direct_gain,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Reverb Send",
        &state.settings.audio.acoustics_reverb_send,
        0.0F,
        1.0F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Reverb Delay",
        &state.settings.audio.acoustics_reverb_delay_ms,
        10.0F,
        300.0F,
        "%.0f ms"
    );
    save_settings |= ImGui::SliderFloat(
        "Reverb Feedback",
        &state.settings.audio.acoustics_reverb_feedback,
        0.0F,
        0.95F,
        "%.2f"
    );
    save_settings |= ImGui::SliderFloat(
        "Reverb Min Cutoff",
        &state.settings.audio.acoustics_reverb_min_cutoff_hz,
        100.0F,
        10000.0F,
        "%.0f Hz"
    );
    save_settings |= ImGui::SliderFloat(
        "Reverb Max Cutoff",
        &state.settings.audio.acoustics_reverb_max_cutoff_hz,
        500.0F,
        20000.0F,
        "%.0f Hz"
    );
    ImGui::Separator();
    ImGui::TextUnformatted("These settings are persisted and affect all positional audio.");
    ImGui::TextUnformatted("Higher half-width = gentler pan. Lower = more aggressive pan.");

    if (peer_admin_disabled) {
        ImGui::EndDisabled();
    }

    if (save_settings && !peer_admin_disabled) {
        SaveSettings(state.settings);
    }

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

void DrawCameraSettingsWindow(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.camera_settings_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(860.0F, 220.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Camera Settings", &debug.camera_settings_window_visible)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginCombo("Mode", CameraModeToString(graphics.camera_mode))) {
        for (int i = 0; i < 2; ++i) {
            const CameraMode mode = static_cast<CameraMode>(i);
            const bool selected = mode == graphics.camera_mode;
            if (ImGui::Selectable(CameraModeToString(mode), selected)) {
                graphics.camera_mode = mode;
                if (mode == CameraMode::StageFit) {
                    graphics.play_cam.pos = GetStageCameraCenter(state.stage);
                }
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Lock Follow Camera", &graphics.debug_lock_play_camera);
    ImGui::Checkbox("Clamp To Stage Bounds", &state.stage.camera_clamp_enabled);
    ImGui::SliderFloat("Follow Zoom", &graphics.follow_camera_zoom, 1.0F, 8.0F, "%.2f");
    ImGui::SliderFloat("Stage Fit Padding", &graphics.stage_fit_padding, 0.0F, 128.0F, "%.1f");
    ImGui::SliderFloat("Zoom Multiplier", &graphics.camera_zoom_multiplier, 0.25F, 4.0F, "%.2f");
    ImGui::SliderFloat("Lerp", &graphics.camera_lerp_factor, 0.01F, 1.0F, "%.2f");

    const Vec2 stage_center = GetStageCameraCenter(state.stage);
    const float stage_fit_zoom = GetStageFitCameraZoom(state.stage, graphics) * graphics.camera_zoom_multiplier;
    const float follow_zoom = GetDefaultFollowCameraZoom(graphics) * graphics.camera_zoom_multiplier;
    ImGui::Text("Current Target: (%.1f, %.1f)", graphics.camera.target.x, graphics.camera.target.y);
    ImGui::Text("Current Zoom: %.2f", graphics.camera.zoom);
    ImGui::Text("Follow Zoom: %.2f", follow_zoom);
    ImGui::Text("Stage Fit Zoom: %.2f", stage_fit_zoom);

    if (ImGui::Button("Snap To Current Mode")) {
        if (graphics.camera_mode == CameraMode::StageFit) {
            graphics.camera.target = stage_center;
            graphics.play_cam.pos = stage_center;
            graphics.camera.zoom = stage_fit_zoom;
        } else {
            graphics.camera.target = graphics.play_cam.pos;
            graphics.camera.zoom = follow_zoom;
        }
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawPerformanceSettingsWindow(DebugPlayback& debug, State& state) {
    if (!debug.performance_settings_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(1080.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Performance", &debug.performance_settings_window_visible)) {
        ImGui::End();
        return;
    }

    const PerformanceStats& perf = state.performance_stats;
    const std::size_t ent_size = sizeof(Ent);
    const std::size_t ent_slots = state.ents.ents.capacity();
    const std::size_t available_id_slots = state.ents.available_ids.capacity();
    const std::size_t ent_storage_bytes = ent_size * ent_slots;
    const std::size_t available_id_storage_bytes = sizeof(std::size_t) * available_id_slots;
    const std::size_t ents_total_bytes = ent_storage_bytes + available_id_storage_bytes;
    const std::size_t particle_count = state.particles.sprite_particles.size() +
                                       state.particles.scripted_particles.size() +
                                       state.particles.ribbon_particles.size() +
                                       state.particles.segmented_sprite_particles.size();
    char buffer_a[64];
    char buffer_b[64];
    char buffer_c[64];

    ImGui::SeparatorText("Frame Timing");
    ImGui::Text("Frame Budget: %.3f ms (%.0f Hz)", perf.frame_budget_ms, 1000.0 / perf.frame_budget_ms);
    DrawTimingRow("Step", perf.step_smoothed_ms, perf.step_ms, perf.step_peak_ms, perf.frame_budget_ms);
    DrawTimingRow(
        "Network Pump",
        perf.network_pump_smoothed_ms,
        perf.network_pump_ms,
        perf.network_pump_peak_ms,
        perf.frame_budget_ms
    );
    DrawTimingRow(
        "Lockstep Hash",
        perf.lockstep_hash_smoothed_ms,
        perf.lockstep_hash_ms,
        perf.lockstep_hash_peak_ms,
        perf.frame_budget_ms
    );
    DrawTimingRow(
        "Rollback Replay",
        perf.rollback_replay_smoothed_ms,
        perf.rollback_replay_ms_this_frame,
        perf.rollback_replay_peak_ms,
        perf.frame_budget_ms
    );
    DrawTimingRow(
        "Multiplayer Sim Total",
        perf.multiplayer_sim_total_smoothed_ms,
        perf.multiplayer_sim_total_ms,
        perf.multiplayer_sim_total_peak_ms,
        perf.frame_budget_ms
    );
    DrawTimingRow("Render", perf.render_smoothed_ms, perf.render_ms, perf.render_peak_ms, perf.frame_budget_ms);
    DrawTimingRow("ImGui", perf.imgui_smoothed_ms, perf.imgui_ms, perf.imgui_peak_ms, perf.frame_budget_ms);
    DrawTimingRow("Present", perf.present_smoothed_ms, perf.present_ms, perf.present_peak_ms, perf.frame_budget_ms);
    DrawTimingRow("Frame Total", perf.frame_total_smoothed_ms, perf.frame_total_ms, perf.frame_total_peak_ms, perf.frame_budget_ms);
    if (ImGui::Button("Reset Timing Peaks")) {
        state.performance_stats.step_peak_ms = state.performance_stats.step_ms;
        state.performance_stats.network_pump_peak_ms = state.performance_stats.network_pump_ms;
        state.performance_stats.lockstep_hash_peak_ms = state.performance_stats.lockstep_hash_ms;
        state.performance_stats.rollback_replay_peak_ms =
            state.performance_stats.rollback_replay_ms_this_frame;
        state.performance_stats.multiplayer_sim_total_peak_ms =
            state.performance_stats.multiplayer_sim_total_ms;
        state.performance_stats.render_peak_ms = state.performance_stats.render_ms;
        state.performance_stats.imgui_peak_ms = state.performance_stats.imgui_ms;
        state.performance_stats.present_peak_ms = state.performance_stats.present_ms;
        state.performance_stats.frame_total_peak_ms = state.performance_stats.frame_total_ms;
    }
    ImGui::Text(
        "Lockstep hash: %u this frame (%u rollback), normal %.3fms rollback %.3fms",
        perf.lockstep_hash_count_this_frame,
        perf.lockstep_hash_rollback_count_this_frame,
        perf.lockstep_hash_normal_ms,
        perf.lockstep_hash_rollback_ms
    );
    ImGui::Text(
        "Rollback replay: %u frames, %.3f ms/frame",
        perf.rollback_replay_frames_this_frame,
        perf.rollback_replay_ms_per_frame
    );
    ImGui::Text(
        "Rollback snapshots: %zu bytes approx, save %.3fms restore %.3fms",
        perf.rollback_buffer_bytes,
        perf.rollback_snapshot_save_ms,
        perf.rollback_snapshot_restore_ms
    );

    ImGui::SeparatorText("Ent Memory");
    ImGui::Text("Ent Size: %zu bytes", ent_size);
    ImGui::Text("Ent Pool: %u / %zu active", state.ents.NumActiveEnts(), ent_slots);
    ImGui::Text("Ent Slots: %zu x %zu = %s", ent_slots, ent_size, FormatBytes(ent_storage_bytes, buffer_a, sizeof(buffer_a)));
    ImGui::Text("Free ID Stack: %zu x %zu = %s", available_id_slots, sizeof(std::size_t), FormatBytes(available_id_storage_bytes, buffer_b, sizeof(buffer_b)));
    ImGui::Text("Ent Manager Storage: %s", FormatBytes(ents_total_bytes, buffer_c, sizeof(buffer_c)));

    ImGui::SeparatorText("Other Counts");
    ImGui::Text("Particles: %zu", particle_count);
    ImGui::Text("Audio Emitters: %zu", state.audio_emitters.emitters.size());
    ImGui::Text("World Prompts: %zu", state.world_prompts.size());

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawPlayerTuningWindow(DebugPlayback& debug, State& state) {
    if (!debug.player_tuning_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(1080.0F, 220.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Player Tuning", &debug.player_tuning_window_visible)) {
        ImGui::End();
        return;
    }

    if (IsPeerMechanicsTuningDisabled(state)) {
        ImGui::TextDisabled("Player tuning disabled on multiplayer peers until mechanics settings are host-routed.");
        ImGui::End();
        SyncDebugUiSettings(debug, state);
        return;
    }

    PlayerTuningState& tuning = state.player_tuning;
    bool changed = false;
    if (ImGui::Button("Reset Current Player Defaults")) {
        tuning = PlayerTuningState{};
        changed = true;
    }

    ImGui::SeparatorText("Vertical");
    changed |= ImGui::DragFloat("Gravity Scale", &tuning.gravity_scale, 0.01F, 0.0F, 4.0F, "%.3f");
    changed |= ImGui::DragFloat("Max Fall Speed", &tuning.max_fall_speed, 0.05F, 0.0F, 20.0F, "%.2f");
    changed |= ImGui::DragFloat("Jump Impulse", &tuning.jump_impulse, 0.05F, 0.0F, 12.0F, "%.2f");
    changed |= ImGui::DragFloat(
        "Spring Shoes Jump Bonus",
        &tuning.spring_shoes_jump_impulse_bonus,
        0.05F,
        0.0F,
        8.0F,
        "%.2f"
    );
    changed |= ImGui::DragInt("Jump Hold Frames", &tuning.jump_hold_frames, 1.0F, 0, 60);
    changed |= ImGui::DragInt("Coyote Frames", &tuning.coyote_frames, 1.0F, 0, 30);
    changed |= ImGui::DragInt("Jump Delay Frames", &tuning.jump_delay_frames, 1.0F, 0, 30);
    changed |= ImGui::DragInt("Fall Light Frames", &tuning.fall_damage_light_frames, 1.0F, 0, 240);
    changed |= ImGui::DragInt("Fall Medium Frames", &tuning.fall_damage_medium_frames, 1.0F, 0, 240);
    changed |= ImGui::DragInt("Fall Heavy Frames", &tuning.fall_damage_heavy_frames, 1.0F, 0, 240);

    ImGui::SeparatorText("Horizontal");
    changed |= ImGui::DragFloat("Walk Speed", &tuning.walk_speed, 0.05F, 0.0F, 12.0F, "%.2f");
    changed |= ImGui::DragFloat("Run Speed", &tuning.run_speed, 0.05F, 0.0F, 12.0F, "%.2f");
    changed |= ImGui::DragFloat("Move Acc", &tuning.move_acc, 0.01F, 0.0F, 4.0F, "%.3f");
    changed |= ImGui::DragFloat("Run Acc", &tuning.run_acc, 0.01F, 0.0F, 4.0F, "%.3f");
    changed |= ImGui::DragFloat("Ground Friction Scale", &tuning.ground_friction_scale, 0.01F, 0.0F, 2.0F, "%.3f");
    changed |= ImGui::DragFloat("Air Friction", &tuning.air_friction, 0.005F, 0.0F, 1.0F, "%.3f");

    ImGui::SeparatorText("Climb / Hang");
    changed |= ImGui::DragFloat("Climb Speed", &tuning.climb_speed, 0.005F, 0.0F, 8.0F, "%.3f");
    changed |= ImGui::DragFloat(
        "Climb Depart Horizontal",
        &tuning.climb_depart_horizontal_speed,
        0.05F,
        0.0F,
        12.0F,
        "%.2f"
    );
    changed |= ImGui::DragFloat("Climb Probe Y Bias", &tuning.climb_probe_bias_pixels, 0.1F, 0.0F, 32.0F, "%.1f");
    changed |= ImGui::DragFloat(
        "Climb Probe X Scale",
        &tuning.climb_probe_x_scale,
        0.01F,
        0.0F,
        3.0F,
        "%.2f"
    );
    changed |= ImGui::DragInt("Climb Required Probe Hits", &tuning.climb_required_probe_hits, 1.0F, 1, 3);
    changed |= ImGui::DragInt("Climb Detach Cooldown", &tuning.climb_detach_cooldown, 1.0F, 0, 60);
    changed |= ImGui::DragInt("Hang Drop Cooldown", &tuning.hang_drop_cooldown, 1.0F, 0, 60);
    changed |= ImGui::DragInt("Glove Hang Drop Cooldown", &tuning.glove_hang_drop_cooldown, 1.0F, 0, 60);
    changed |= ImGui::DragInt("Hang Wall Release Cooldown", &tuning.hang_wall_release_cooldown, 1.0F, 0, 60);
    changed |= ImGui::Checkbox("Auto Ledge Grab", &tuning.auto_ledge_grab);

    if (changed) {
        state.settings.player_tuning = state.player_tuning;
        SaveSettings(state.settings);
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawPostFxSettingsWindow(DebugPlayback& debug, State& state, const Graphics& graphics) {
    if (!debug.post_fx_settings_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(1100.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Post FX Settings", &debug.post_fx_settings_window_visible)) {
        ImGui::End();
        return;
    }

    bool changed = false;
    int effect = static_cast<int>(state.settings.post_process.effect);
    const char* effect_names[] = {"None", "CRT"};
    if (ImGui::Combo("Effect", &effect, effect_names, IM_ARRAYSIZE(effect_names))) {
        state.settings.post_process.effect = static_cast<PostProcessEffect>(effect);
        changed = true;
    }

    if (state.settings.post_process.effect == PostProcessEffect::Crt) {
        changed |= ImGui::SliderFloat(
            "Scanlines",
            &state.settings.post_process.crt_scanline_amount,
            0.0F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Scanline Edge Start",
            &state.settings.post_process.crt_scanline_edge_start,
            0.0F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Scanline Edge Falloff",
            &state.settings.post_process.crt_scanline_edge_falloff,
            0.01F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Scanline Edge Strength",
            &state.settings.post_process.crt_scanline_edge_strength,
            0.0F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Zoom",
            &state.settings.post_process.crt_zoom,
            0.50F,
            1.50F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Warp",
            &state.settings.post_process.crt_warp_amount,
            0.0F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Vignette",
            &state.settings.post_process.crt_vignette_amount,
            0.0F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Vignette Intensity",
            &state.settings.post_process.crt_vignette_intensity,
            0.0F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Grille",
            &state.settings.post_process.crt_grille_amount,
            0.0F,
            1.0F,
            "%.2f"
        );
        changed |= ImGui::SliderFloat(
            "Brightness",
            &state.settings.post_process.crt_brightness_boost,
            1.0F,
            2.0F,
            "%.2f"
        );
    }

    ImGui::TextUnformatted(
        graphics.gpu_renderer_active
            ? "Renderer: SDL GPU"
            : "Renderer: fallback (post FX unavailable)"
    );

    if (changed) {
        SaveSettings(state.settings);
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawLightingSettingsWindow(DebugPlayback& debug, State& state, Graphics& graphics) {
    (void)graphics;
    if (!debug.lighting_settings_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(1100.0F, 280.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Lighting Settings", &debug.lighting_settings_window_visible)) {
        ImGui::End();
        return;
    }

    bool changed = false;
    changed |=
        ImGui::Checkbox("Terrain Lighting", &state.settings.post_process.terrain_lighting);
    changed |= ImGui::Checkbox("Terrain Seam AO", &state.settings.post_process.terrain_seam_ao);
    changed |= ImGui::SliderFloat(
        "Terrain Seam AO Amount",
        &state.settings.post_process.terrain_seam_ao_amount,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Seam AO Size",
        &state.settings.post_process.terrain_seam_ao_size,
        0.05F,
        0.50F,
        "%.2f"
    );
    changed |= ImGui::Checkbox(
        "Terrain Exposure Lighting",
        &state.settings.post_process.terrain_exposure_lighting
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Amount",
        &state.settings.post_process.terrain_exposure_amount,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Diagonal Weight",
        &state.settings.post_process.terrain_exposure_diagonal_weight,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Smoothing",
        &state.settings.post_process.terrain_exposure_smoothing,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::Checkbox(
        "Terrain Exposure Remap Enabled",
        &state.settings.post_process.terrain_exposure_remap_enabled
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Input Min",
        &state.settings.post_process.terrain_exposure_input_min,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Input Max",
        &state.settings.post_process.terrain_exposure_input_max,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Gamma",
        &state.settings.post_process.terrain_exposure_gamma,
        0.10F,
        4.00F,
        "%.2f"
    );
    changed |= ImGui::Checkbox(
        "Terrain Exposure Output Levels Enabled",
        &state.settings.post_process.terrain_exposure_output_levels_enabled
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Min Brightness",
        &state.settings.post_process.terrain_exposure_min_brightness,
        0.0F,
        2.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Exposure Max Brightness",
        &state.settings.post_process.terrain_exposure_max_brightness,
        0.0F,
        2.0F,
        "%.2f"
    );
    changed |= ImGui::Checkbox("Backwall Lighting", &state.settings.post_process.backwall_lighting);
    changed |= ImGui::SliderFloat(
        "Player Lamp Strength",
        &state.settings.post_process.player_lamp_strength,
        0.0F,
        4.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Embedded Treasure Brightness",
        &state.settings.post_process.embedded_treasure_brightness,
        0.0F,
        2.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Openness Ambient Strength",
        &state.settings.post_process.openness_ambient_strength,
        0.0F,
        0.50F,
        "%.3f"
    );
    changed |= ImGui::SliderFloat(
        "Openness Ambient Gamma",
        &state.settings.post_process.openness_ambient_gamma,
        0.10F,
        4.00F,
        "%.2f"
    );
    changed |= ImGui::Checkbox(
        "Lighting Temporal Smoothing",
        &state.settings.post_process.lighting_temporal_smoothing
    );
    changed |= ImGui::SliderFloat(
        "Lighting Temporal Response",
        &state.settings.post_process.lighting_temporal_smoothing_response,
        0.01F,
        1.00F,
        "%.2f"
    );
    ImGui::TextUnformatted("Response: 1.0 = immediate, 0.1 = 10% toward target per frame.");
    changed |= ImGui::SliderFloat(
        "Backwall Smoothing",
        &state.settings.post_process.backwall_smoothing,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::Checkbox(
        "Backwall Remap Enabled",
        &state.settings.post_process.backwall_remap_enabled
    );
    changed |= ImGui::SliderFloat(
        "Backwall Input Min",
        &state.settings.post_process.backwall_input_min,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Backwall Input Max",
        &state.settings.post_process.backwall_input_max,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Backwall Gamma",
        &state.settings.post_process.backwall_gamma,
        0.10F,
        4.00F,
        "%.2f"
    );
    changed |= ImGui::Checkbox(
        "Backwall Output Levels Enabled",
        &state.settings.post_process.backwall_output_levels_enabled
    );
    changed |= ImGui::SliderFloat(
        "Backwall Brightness",
        &state.settings.post_process.backwall_brightness,
        0.0F,
        2.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Backwall Min Brightness",
        &state.settings.post_process.backwall_min_brightness,
        0.0F,
        2.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Backwall Max Brightness",
        &state.settings.post_process.backwall_max_brightness,
        0.0F,
        2.0F,
        "%.2f"
    );

    if (changed) {
        InvalidateStageLighting(state);
        SaveSettings(state.settings);
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

void DrawGraphicsSettingsWindow(
    DebugPlayback& debug,
    State& state,
    Graphics& graphics,
    SDL_Window* window,
    SDL_Renderer* renderer
) {
    (void)window;

    if (!debug.graphics_settings_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(1340.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Graphics Settings", &debug.graphics_settings_window_visible)) {
        ImGui::End();
        return;
    }

    bool changed = false;

    ImGui::TextUnformatted(graphics.gpu_renderer_active ? "Renderer: SDL GPU" : "Renderer: SDL Renderer");
    ImGui::Text(
        "Window Size: %u x %u",
        static_cast<unsigned int>(graphics.window_dims.x),
        static_cast<unsigned int>(graphics.window_dims.y)
    );
    ImGui::Text(
        "Internal Resolution: %u x %u",
        static_cast<unsigned int>(graphics.dims.x),
        static_cast<unsigned int>(graphics.dims.y)
    );
    ImGui::Text("Fullscreen: %s", graphics.fullscreen ? "On" : "Off");

    bool vsync = state.settings.video.vsync;
    if (ImGui::Checkbox("VSync", &vsync)) {
        state.settings.video.vsync = vsync;
        if (renderer != nullptr) {
            SDL_SetRenderVSync(renderer, vsync ? 1 : 0);
        }
        changed = true;
    }

    if (ImGui::Button("Match Internal To Window")) {
        graphics.dims = graphics.window_dims;
        state.settings.video.resolution = graphics.dims;
        state.rebuild_render_texture = true;
        changed = true;
    }

    ImGui::SeparatorText("Frame Data");
    ImGui::Text("Annotations: %s", graphics.aframe_annotations_path.c_str());
    if (ImGui::Button("Reload Frame Data")) {
        if (graphics.ReloadAFrame(renderer, &debug.aframe_reload_status)) {
            ents::common::RefreshAllEntAFrameGeometry(state, graphics);
            state.RebuildSid(graphics);
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Reload", &debug.aframe_auto_reload);
    if (!debug.aframe_reload_status.empty()) {
        ImGui::TextWrapped("%s", debug.aframe_reload_status.c_str());
    }

    if (changed) {
        SaveSettings(state.settings);
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

} // namespace splonks::debug_playback_internal

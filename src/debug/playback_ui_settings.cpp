#include "debug/playback_internal.hpp"

#include "settings.hpp"
#include "stage_lighting.hpp"

#include <imgui.h>

namespace splonks::debug_playback_internal {

namespace {

const char* CameraModeToString(CameraMode mode) {
    switch (mode) {
    case CameraMode::Follow:
        return "Follow";
    case CameraMode::StageFit:
        return "StageFit";
    }
    return "Unknown";
}

} // namespace

void DrawDebugOverlayWindow(DebugPlayback& debug, State& state, Graphics&) {
    if (!debug.entity_annotations_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(620.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Overlay", &debug.entity_annotations_visible)) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Show Entity P/C Boxes", &state.debug_overlay.show_entity_collision_boxes);
    ImGui::Checkbox("Show Entity IDs", &state.debug_overlay.show_entity_ids);
    ImGui::Checkbox("Show Entity Types", &state.debug_overlay.show_entity_types);
    ImGui::Checkbox("Show Void Death Line", &state.debug_overlay.show_void_death_line);
    ImGui::Checkbox("Show Chunk Boundaries", &state.debug_overlay.show_chunk_boundaries);
    ImGui::Checkbox("Show Chunk Coords", &state.debug_overlay.show_chunk_coords);
    ImGui::Checkbox("Show Tile Indexes", &state.debug_overlay.show_tile_indexes);
    ImGui::Checkbox("Show Tile Types", &state.debug_overlay.show_tile_types);
    ImGui::Checkbox("Show Lights", &state.debug_overlay.show_lights);
    ImGui::Checkbox("Show Area Boundaries", &state.debug_overlay.show_area_boundaries);
    ImGui::Checkbox("Show Area IDs", &state.debug_overlay.show_area_ids);
    ImGui::Checkbox("Show Area Types", &state.debug_overlay.show_area_types);
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
    ImGui::Checkbox("Affect Entities", &state.debug_shake_brush.affect_entities);
    ImGui::SliderFloat(
        "Entity Amount (px)",
        &state.debug_shake_brush.entity_amount,
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
    changed |= ImGui::Checkbox(
        "Terrain Face Shading",
        &state.settings.post_process.terrain_face_shading
    );
    changed |= ImGui::Checkbox(
        "Enclosed Stage Bounds",
        &state.settings.post_process.terrain_face_enclosed_stage_bounds
    );
    changed |= ImGui::SliderFloat(
        "Terrain Top Highlight",
        &state.settings.post_process.terrain_face_top_highlight,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Side Shade",
        &state.settings.post_process.terrain_face_side_shade,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Bottom Shade",
        &state.settings.post_process.terrain_face_bottom_shade,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Band Size",
        &state.settings.post_process.terrain_face_band_size,
        0.05F,
        0.50F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Gradient Softness",
        &state.settings.post_process.terrain_face_gradient_softness,
        0.0F,
        1.0F,
        "%.2f"
    );
    changed |= ImGui::SliderFloat(
        "Terrain Corner Rounding",
        &state.settings.post_process.terrain_face_corner_rounding,
        0.0F,
        1.0F,
        "%.2f"
    );
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

    if (changed) {
        SaveSettings(state.settings);
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

} // namespace splonks::debug_playback_internal

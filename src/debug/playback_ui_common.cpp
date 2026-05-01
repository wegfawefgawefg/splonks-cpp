#include "debug/playback_internal.hpp"

#include "debug/playback.hpp"
#include "settings.hpp"
#include "state.hpp"

#include <algorithm>

namespace splonks::debug_playback_internal {

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
    if (state.settings.debug_ui.shake_brush_visible != debug.shake_brush_window_visible) {
        state.settings.debug_ui.shake_brush_visible = debug.shake_brush_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.audio_brush_visible != debug.audio_brush_window_visible) {
        state.settings.debug_ui.audio_brush_visible = debug.audio_brush_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_visible != debug.fluid_brush_window_visible) {
        state.settings.debug_ui.fluid_brush_visible = debug.fluid_brush_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_enabled != state.debug_fluid_brush.enabled) {
        state.settings.debug_ui.fluid_brush_enabled = state.debug_fluid_brush.enabled;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_simulation_enabled !=
        state.debug_fluid_brush.simulation_enabled) {
        state.settings.debug_ui.fluid_brush_simulation_enabled =
            state.debug_fluid_brush.simulation_enabled;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_replace_solid_tiles !=
        state.debug_fluid_brush.replace_solid_tiles) {
        state.settings.debug_ui.fluid_brush_replace_solid_tiles =
            state.debug_fluid_brush.replace_solid_tiles;
        changed = true;
    }
    const int fluid_brush_radius_tiles = std::max(0, state.debug_fluid_brush.radius_tiles);
    if (state.settings.debug_ui.fluid_brush_radius_tiles != fluid_brush_radius_tiles) {
        state.settings.debug_ui.fluid_brush_radius_tiles = fluid_brush_radius_tiles;
        changed = true;
    }
    const int fluid_brush_simulation_interval_frames =
        std::max(1, state.debug_fluid_brush.simulation_interval_frames);
    if (state.settings.debug_ui.fluid_brush_simulation_interval_frames !=
        fluid_brush_simulation_interval_frames) {
        state.settings.debug_ui.fluid_brush_simulation_interval_frames =
            fluid_brush_simulation_interval_frames;
        changed = true;
    }
    const int fluid_brush_vertical_transfer_per_step =
        std::clamp(state.debug_fluid_brush.vertical_transfer_per_step, 0, 255);
    if (state.settings.debug_ui.fluid_brush_vertical_transfer_per_step !=
        fluid_brush_vertical_transfer_per_step) {
        state.settings.debug_ui.fluid_brush_vertical_transfer_per_step =
            fluid_brush_vertical_transfer_per_step;
        changed = true;
    }
    const int fluid_brush_horizontal_transfer_per_step =
        std::clamp(state.debug_fluid_brush.horizontal_transfer_per_step, 0, 255);
    if (state.settings.debug_ui.fluid_brush_horizontal_transfer_per_step !=
        fluid_brush_horizontal_transfer_per_step) {
        state.settings.debug_ui.fluid_brush_horizontal_transfer_per_step =
            fluid_brush_horizontal_transfer_per_step;
        changed = true;
    }
    const int fluid_brush_horizontal_flow_deadband =
        std::clamp(state.debug_fluid_brush.horizontal_flow_deadband, 0, 255);
    if (state.settings.debug_ui.fluid_brush_horizontal_flow_deadband !=
        fluid_brush_horizontal_flow_deadband) {
        state.settings.debug_ui.fluid_brush_horizontal_flow_deadband =
            fluid_brush_horizontal_flow_deadband;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_render_blur_enabled !=
        state.debug_fluid_brush.render_blur_enabled) {
        state.settings.debug_ui.fluid_brush_render_blur_enabled =
            state.debug_fluid_brush.render_blur_enabled;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_temporal_smoothing_enabled !=
        state.debug_fluid_brush.temporal_smoothing_enabled) {
        state.settings.debug_ui.fluid_brush_temporal_smoothing_enabled =
            state.debug_fluid_brush.temporal_smoothing_enabled;
        changed = true;
    }
    const float fluid_brush_temporal_smoothing_response = std::clamp(
        state.debug_fluid_brush.temporal_smoothing_response,
        0.0F,
        1.0F
    );
    if (state.settings.debug_ui.fluid_brush_temporal_smoothing_response !=
        fluid_brush_temporal_smoothing_response) {
        state.settings.debug_ui.fluid_brush_temporal_smoothing_response =
            fluid_brush_temporal_smoothing_response;
        changed = true;
    }
    const float fluid_brush_render_cutoff_amount = std::clamp(
        state.debug_fluid_brush.render_cutoff_amount,
        0.0F,
        255.0F
    );
    if (state.settings.debug_ui.fluid_brush_render_cutoff_amount !=
        fluid_brush_render_cutoff_amount) {
        state.settings.debug_ui.fluid_brush_render_cutoff_amount =
            fluid_brush_render_cutoff_amount;
        changed = true;
    }
    if (state.settings.debug_ui.audio_settings_visible != debug.audio_settings_window_visible) {
        state.settings.debug_ui.audio_settings_visible = debug.audio_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.ui_settings_visible != debug.ui_settings_window_visible) {
        state.settings.debug_ui.ui_settings_visible = debug.ui_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.post_fx_settings_visible != debug.post_fx_settings_window_visible) {
        state.settings.debug_ui.post_fx_settings_visible = debug.post_fx_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.lighting_settings_visible != debug.lighting_settings_window_visible) {
        state.settings.debug_ui.lighting_settings_visible = debug.lighting_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.graphics_settings_visible != debug.graphics_settings_window_visible) {
        state.settings.debug_ui.graphics_settings_visible = debug.graphics_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.camera_settings_visible != debug.camera_settings_window_visible) {
        state.settings.debug_ui.camera_settings_visible = debug.camera_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.performance_settings_visible != debug.performance_settings_window_visible) {
        state.settings.debug_ui.performance_settings_visible = debug.performance_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.player_tuning_visible != debug.player_tuning_window_visible) {
        state.settings.debug_ui.player_tuning_visible = debug.player_tuning_window_visible;
        changed = true;
    }
    const std::uint32_t character_swap_type =
        static_cast<std::uint32_t>(EntityTypeIndex(debug.character_swap_entity_type));
    if (state.settings.debug_ui.entity_swap_type != character_swap_type) {
        state.settings.debug_ui.entity_swap_type = character_swap_type;
        changed = true;
    }
    const std::uint32_t default_spawn_type =
        static_cast<std::uint32_t>(EntityTypeIndex(debug.default_spawn_entity_type));
    if (state.settings.debug_ui.default_spawn_type != default_spawn_type) {
        state.settings.debug_ui.default_spawn_type = default_spawn_type;
        changed = true;
    }
    if (state.settings.debug_ui.default_spawn_enabled != debug.default_spawn_enabled) {
        state.settings.debug_ui.default_spawn_enabled = debug.default_spawn_enabled;
        changed = true;
    }
    if (state.settings.debug_ui.entity_swap_fresh != debug.character_swap_fresh) {
        state.settings.debug_ui.entity_swap_fresh = debug.character_swap_fresh;
        changed = true;
    }
    if (state.settings.debug_ui.entity_swap_keep_passives != debug.character_swap_keep_passives) {
        state.settings.debug_ui.entity_swap_keep_passives = debug.character_swap_keep_passives;
        changed = true;
    }
    if (state.settings.debug_ui.entity_swap_keep_money != debug.character_swap_keep_money) {
        state.settings.debug_ui.entity_swap_keep_money = debug.character_swap_keep_money;
        changed = true;
    }
    if (state.settings.debug_ui.entity_swap_keep_health != debug.character_swap_keep_health) {
        state.settings.debug_ui.entity_swap_keep_health = debug.character_swap_keep_health;
        changed = true;
    }
    if (state.settings.debug_ui.entity_swap_keep_tools != debug.character_swap_keep_tools) {
        state.settings.debug_ui.entity_swap_keep_tools = debug.character_swap_keep_tools;
        changed = true;
    }

    if (changed) {
        SaveSettings(state.settings);
    }

    return changed;
}

} // namespace splonks::debug_playback_internal

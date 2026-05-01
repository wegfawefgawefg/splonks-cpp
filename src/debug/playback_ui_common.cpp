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
    const int fluid_brush_mode = std::clamp(
        static_cast<int>(state.debug_fluid_brush.mode),
        0,
        3
    );
    if (state.settings.debug_ui.fluid_brush_mode != fluid_brush_mode) {
        state.settings.debug_ui.fluid_brush_mode = fluid_brush_mode;
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
    const float fluid_brush_transfer_per_step =
        std::clamp(state.debug_fluid_brush.transfer_per_step, 0.0F, 1.0F);
    if (state.settings.debug_ui.fluid_brush_transfer_per_step !=
        fluid_brush_transfer_per_step) {
        state.settings.debug_ui.fluid_brush_transfer_per_step =
            fluid_brush_transfer_per_step;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_gravity_x != state.debug_fluid_brush.gravity_x) {
        state.settings.debug_ui.fluid_brush_gravity_x = state.debug_fluid_brush.gravity_x;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_gravity_y != state.debug_fluid_brush.gravity_y) {
        state.settings.debug_ui.fluid_brush_gravity_y = state.debug_fluid_brush.gravity_y;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_paint_gravity_x !=
        state.debug_fluid_brush.paint_gravity_x) {
        state.settings.debug_ui.fluid_brush_paint_gravity_x =
            state.debug_fluid_brush.paint_gravity_x;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_paint_gravity_y !=
        state.debug_fluid_brush.paint_gravity_y) {
        state.settings.debug_ui.fluid_brush_paint_gravity_y =
            state.debug_fluid_brush.paint_gravity_y;
        changed = true;
    }
    const float fluid_brush_pressure_strength = std::clamp(
        state.debug_fluid_brush.pressure_strength,
        0.0F,
        4.0F
    );
    if (state.settings.debug_ui.fluid_brush_pressure_strength !=
        fluid_brush_pressure_strength) {
        state.settings.debug_ui.fluid_brush_pressure_strength =
            fluid_brush_pressure_strength;
        changed = true;
    }
    const float fluid_brush_velocity_damping = std::clamp(
        state.debug_fluid_brush.velocity_damping,
        0.0F,
        1.0F
    );
    if (state.settings.debug_ui.fluid_brush_velocity_damping !=
        fluid_brush_velocity_damping) {
        state.settings.debug_ui.fluid_brush_velocity_damping =
            fluid_brush_velocity_damping;
        changed = true;
    }
    const float fluid_brush_temp_gravity_decay = std::clamp(
        state.debug_fluid_brush.temp_gravity_decay,
        0.0F,
        1.0F
    );
    if (state.settings.debug_ui.fluid_brush_temp_gravity_decay !=
        fluid_brush_temp_gravity_decay) {
        state.settings.debug_ui.fluid_brush_temp_gravity_decay =
            fluid_brush_temp_gravity_decay;
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
        1.0F
    );
    if (state.settings.debug_ui.fluid_brush_render_cutoff_amount !=
        fluid_brush_render_cutoff_amount) {
        state.settings.debug_ui.fluid_brush_render_cutoff_amount =
            fluid_brush_render_cutoff_amount;
        changed = true;
    }
    const float fluid_brush_topper_cutoff_amount = std::clamp(
        state.debug_fluid_brush.topper_cutoff_amount,
        0.0F,
        1.0F
    );
    if (state.settings.debug_ui.fluid_brush_topper_cutoff_amount !=
        fluid_brush_topper_cutoff_amount) {
        state.settings.debug_ui.fluid_brush_topper_cutoff_amount =
            fluid_brush_topper_cutoff_amount;
        changed = true;
    }
    if (state.settings.debug_ui.fluid_brush_show_flow_indicators !=
        state.debug_fluid_brush.show_flow_indicators) {
        state.settings.debug_ui.fluid_brush_show_flow_indicators =
            state.debug_fluid_brush.show_flow_indicators;
        changed = true;
    }
    const int fluid_brush_render_mode = std::clamp(
        static_cast<int>(state.debug_fluid_brush.render_mode),
        0,
        1
    );
    if (state.settings.debug_ui.fluid_brush_render_mode != fluid_brush_render_mode) {
        state.settings.debug_ui.fluid_brush_render_mode = fluid_brush_render_mode;
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

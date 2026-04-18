#include "debug/playback.hpp"

namespace splonks {

GameplaySnapshot MakeGameplaySnapshot(const State& state, const Graphics& graphics) {
    GameplaySnapshot snapshot;
    snapshot.mode = state.mode;
    snapshot.settings = state.settings;
    snapshot.menu_inputs = state.menu_inputs;
    snapshot.menu_input_snapshot = state.menu_input_snapshot;
    snapshot.previous_menu_input_snapshot = state.previous_menu_input_snapshot;
    snapshot.menu_input_debounce_timers = state.menu_input_debounce_timers;
    snapshot.playing_inputs = state.playing_inputs;
    snapshot.immediate_playing_inputs = state.immediate_playing_inputs;
    snapshot.playing_input_snapshot = state.playing_input_snapshot;
    snapshot.previous_playing_input_snapshot = state.previous_playing_input_snapshot;
    snapshot.previous_immediate_playing_input_snapshot =
        state.previous_immediate_playing_input_snapshot;
    snapshot.title_menu_selection = state.title_menu_selection;
    snapshot.settings_menu_selection = state.settings_menu_selection;
    snapshot.video_settings_menu_selection = state.video_settings_menu_selection;
    snapshot.ui_settings_menu_selection = state.ui_settings_menu_selection;
    snapshot.post_fx_settings_menu_selection = state.post_fx_settings_menu_selection;
    snapshot.lighting_settings_menu_selection = state.lighting_settings_menu_selection;
    snapshot.video_settings_target_window_size_index = state.video_settings_target_window_size_index;
    snapshot.video_settings_target_resolution_index = state.video_settings_target_resolution_index;
    snapshot.video_settings_target_fullscreen = state.video_settings_target_fullscreen;
    snapshot.rebuild_render_texture = state.rebuild_render_texture;
    snapshot.choosing_control_binding = state.choosing_control_binding;
    snapshot.debug_overlay = state.debug_overlay;
    snapshot.debug_shake_brush = state.debug_shake_brush;
    snapshot.now = state.now;
    snapshot.time_since_last_update = state.time_since_last_update;
    snapshot.scene_frame = state.scene_frame;
    snapshot.frame = state.frame;
    snapshot.stage_frame = state.stage_frame;
    snapshot.menu_return_to = state.menu_return_to;
    snapshot.game_over = state.game_over;
    snapshot.pause = state.pause;
    snapshot.win = state.win;
    snapshot.respawn_target = state.respawn_target;
    snapshot.pending_stage_transition = state.pending_stage_transition;
    snapshot.points = state.points;
    snapshot.deaths = state.deaths;
    snapshot.depth = state.depth;
    snapshot.frame_pause = state.frame_pause;
    snapshot.debug_level = state.debug_level;
    snapshot.entity_manager = state.entity_manager;
    snapshot.stage = state.stage;
    snapshot.player_vid = state.player_vid;
    snapshot.controlled_entity_vid = state.controlled_entity_vid;
    snapshot.mouse_trailer_vid = state.mouse_trailer_vid;
    snapshot.entity_tool_states = state.entity_tools.tool_states;
    snapshot.play_cam_pos = graphics.play_cam.pos;
    return snapshot;
}

void RestoreGameplaySnapshot(const GameplaySnapshot& snapshot, State& state, Graphics& graphics) {
    state.mode = snapshot.mode;
    state.settings = snapshot.settings;
    state.menu_inputs = snapshot.menu_inputs;
    state.menu_input_snapshot = snapshot.menu_input_snapshot;
    state.previous_menu_input_snapshot = snapshot.previous_menu_input_snapshot;
    state.menu_input_debounce_timers = snapshot.menu_input_debounce_timers;
    state.playing_inputs = snapshot.playing_inputs;
    state.immediate_playing_inputs = snapshot.immediate_playing_inputs;
    state.playing_input_snapshot = snapshot.playing_input_snapshot;
    state.previous_playing_input_snapshot = snapshot.previous_playing_input_snapshot;
    state.previous_immediate_playing_input_snapshot =
        snapshot.previous_immediate_playing_input_snapshot;
    state.title_menu_selection = snapshot.title_menu_selection;
    state.settings_menu_selection = snapshot.settings_menu_selection;
    state.video_settings_menu_selection = snapshot.video_settings_menu_selection;
    state.ui_settings_menu_selection = snapshot.ui_settings_menu_selection;
    state.post_fx_settings_menu_selection = snapshot.post_fx_settings_menu_selection;
    state.lighting_settings_menu_selection = snapshot.lighting_settings_menu_selection;
    state.video_settings_target_window_size_index = snapshot.video_settings_target_window_size_index;
    state.video_settings_target_resolution_index = snapshot.video_settings_target_resolution_index;
    state.video_settings_target_fullscreen = snapshot.video_settings_target_fullscreen;
    state.rebuild_render_texture = snapshot.rebuild_render_texture;
    state.choosing_control_binding = snapshot.choosing_control_binding;
    state.debug_overlay = snapshot.debug_overlay;
    state.debug_shake_brush = snapshot.debug_shake_brush;
    state.now = snapshot.now;
    state.time_since_last_update = snapshot.time_since_last_update;
    state.scene_frame = snapshot.scene_frame;
    state.frame = snapshot.frame;
    state.stage_frame = snapshot.stage_frame;
    state.menu_return_to = snapshot.menu_return_to;
    state.game_over = snapshot.game_over;
    state.pause = snapshot.pause;
    state.win = snapshot.win;
    state.respawn_target = snapshot.respawn_target;
    state.pending_stage_transition = snapshot.pending_stage_transition;
    state.points = snapshot.points;
    state.deaths = snapshot.deaths;
    state.depth = snapshot.depth;
    state.frame_pause = snapshot.frame_pause;
    state.debug_level = snapshot.debug_level;
    state.entity_manager = snapshot.entity_manager;
    state.particles.Clear();
    state.stage = snapshot.stage;
    state.player_vid = snapshot.player_vid;
    state.controlled_entity_vid = snapshot.controlled_entity_vid;
    state.mouse_trailer_vid = snapshot.mouse_trailer_vid;
    state.entity_tools.tool_states = snapshot.entity_tool_states;
    state.RebuildSid(graphics);
    graphics.play_cam.pos = snapshot.play_cam_pos;
}

} // namespace splonks

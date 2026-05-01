#pragma once

#include "math_types.hpp"

#include <cstdint>
#include <vector>

namespace splonks {

enum class SettingsMode {
    Main,
    Video,
    Audio,
    Controls,
};

struct VideoSettings {
    UVec2 resolution;
    bool fullscreen = false;
    bool vsync = false;
    std::vector<UVec2> resolution_options;

    static VideoSettings New();
};

struct AudioSettings {
    float music_volume = 1.0F;
    float sfx_volume = 1.0F;
    float pan_half_width_px = 256.0F;
    bool acoustics_enabled = true;
    float acoustics_occlusion_listener_epsilon_px = 4.0F;
    bool acoustics_reverb_enabled = true;
    float acoustics_listener_room_weight = 0.65F;
    float acoustics_direct_min_cutoff_hz = 1200.0F;
    float acoustics_direct_max_cutoff_hz = 16000.0F;
    float acoustics_occluded_cutoff_hz = 900.0F;
    float acoustics_occluded_direct_gain = 0.55F;
    float acoustics_reverb_send = 0.35F;
    float acoustics_reverb_delay_ms = 90.0F;
    float acoustics_reverb_feedback = 0.45F;
    float acoustics_reverb_min_cutoff_hz = 1200.0F;
    float acoustics_reverb_max_cutoff_hz = 4200.0F;

    static AudioSettings New();
};

struct ControlsSettings {
    unsigned int jump = 0;
    unsigned int shoot = 1;

    static ControlsSettings New();
};

struct UiSettings {
    float icon_scale = 1.0F;
    float status_icon_scale = 1.0F;
    float tool_slot_scale = 1.0F;
    float tool_icon_scale = 1.0F;

    static UiSettings New();
};

enum class PostProcessEffect : std::uint8_t {
    None,
    Crt,
};

struct PostProcessSettings {
    PostProcessEffect effect = PostProcessEffect::Crt;
    bool terrain_lighting = true;
    bool terrain_face_shading = true;
    bool terrain_face_enclosed_stage_bounds = true;
    bool terrain_seam_ao = true;
    bool terrain_exposure_lighting = true;
    bool backwall_lighting = true;
    float terrain_face_top_highlight = 0.18F;
    float terrain_face_side_shade = 0.12F;
    float terrain_face_bottom_shade = 0.20F;
    float terrain_face_band_size = 0.22F;
    float terrain_face_gradient_softness = 0.75F;
    float terrain_face_corner_rounding = 0.80F;
    float terrain_seam_ao_amount = 0.18F;
    float terrain_seam_ao_size = 0.20F;
    float terrain_exposure_amount = 0.12F;
    bool terrain_exposure_remap_enabled = false;
    float terrain_exposure_input_min = 0.0F;
    float terrain_exposure_input_max = 1.0F;
    float terrain_exposure_gamma = 1.0F;
    bool terrain_exposure_output_levels_enabled = true;
    float terrain_exposure_min_brightness = 0.50F;
    float terrain_exposure_max_brightness = 1.50F;
    float terrain_exposure_diagonal_weight = 0.50F;
    float terrain_exposure_smoothing = 0.70F;
    float backwall_brightness = 0.85F;
    bool backwall_remap_enabled = false;
    float backwall_input_min = 0.0F;
    float backwall_input_max = 1.0F;
    float backwall_gamma = 1.0F;
    bool backwall_output_levels_enabled = true;
    float backwall_min_brightness = 0.55F;
    float backwall_max_brightness = 1.00F;
    float backwall_smoothing = 0.85F;
    float crt_scanline_amount = 0.5F;
    float crt_scanline_edge_start = 0.35F;
    float crt_scanline_edge_falloff = 0.25F;
    float crt_scanline_edge_strength = 1.0F;
    float crt_zoom = 1.0F;
    float crt_warp_amount = 0.05F;
    float crt_vignette_amount = 0.5F;
    float crt_vignette_intensity = 0.3F;
    float crt_grille_amount = 0.05F;
    float crt_brightness_boost = 1.2F;

    static PostProcessSettings New();
};

struct DebugUiSettings {
    bool menu_visible = true;
    bool playback_visible = true;
    bool level_visible = true;
    bool entities_visible = true;
    bool entity_annotations_visible = false;
    bool shake_brush_visible = false;
    bool audio_brush_visible = false;
    bool fluid_brush_visible = false;
    bool fluid_brush_enabled = false;
    bool fluid_brush_simulation_enabled = true;
    bool fluid_brush_replace_solid_tiles = false;
    int fluid_brush_radius_tiles = 1;
    int fluid_brush_simulation_interval_frames = 6;
    int fluid_brush_vertical_transfer_per_step = 255;
    int fluid_brush_horizontal_transfer_per_step = 255;
    int fluid_brush_horizontal_flow_deadband = 1;
    bool fluid_brush_render_blur_enabled = true;
    bool fluid_brush_temporal_smoothing_enabled = false;
    float fluid_brush_temporal_smoothing_response = 0.35F;
    float fluid_brush_render_cutoff_amount = 1.0F;
    bool audio_settings_visible = false;
    bool ui_settings_visible = false;
    bool post_fx_settings_visible = false;
    bool lighting_settings_visible = false;
    bool graphics_settings_visible = false;
    bool camera_settings_visible = false;
    bool performance_settings_visible = false;
    bool player_tuning_visible = false;
    std::uint32_t entity_swap_type = 1;
    std::uint32_t default_spawn_type = 1;
    bool default_spawn_enabled = false;
    bool entity_swap_fresh = true;
    bool entity_swap_keep_passives = false;
    bool entity_swap_keep_money = false;
    bool entity_swap_keep_health = false;
    bool entity_swap_keep_tools = false;

    static DebugUiSettings New();
};

struct PlayerTuningState {
    float gravity_scale = 1.0F;
    float max_fall_speed = 9.0F;
    float jump_impulse = 4.5F;
    float spring_shoes_jump_impulse_bonus = 1.0F;
    int jump_hold_frames = 0;
    int coyote_frames = 6;
    int jump_delay_frames = 1;
    int fall_damage_light_frames = 32;
    int fall_damage_medium_frames = 64;
    int fall_damage_heavy_frames = 96;

    float walk_speed = 2.5F;
    float run_speed = 4.0F;
    float move_acc = 0.5F;
    float run_acc = 0.5F;
    float ground_friction_scale = 1.0F;
    float air_friction = 0.85F;

    float climb_speed = 3.0F;
    float climb_depart_horizontal_speed = 4.0F;
    float climb_probe_bias_pixels = 8.0F;
    float climb_probe_x_scale = 0.5F;
    int climb_required_probe_hits = 2;
    int climb_detach_cooldown = 5;
    int hang_drop_cooldown = 5;
    int glove_hang_drop_cooldown = 10;
    int hang_wall_release_cooldown = 4;
    bool auto_ledge_grab = true;
};

struct Settings {
    SettingsMode mode = SettingsMode::Main;
    VideoSettings video;
    AudioSettings audio;
    ControlsSettings controls;
    UiSettings ui;
    PostProcessSettings post_process;
    DebugUiSettings debug_ui;
    PlayerTuningState player_tuning;

    static Settings New();
};

constexpr float kKeyDebounceInterval = 0.2F;

Settings LoadSettings();
bool SaveSettings(const Settings& settings);

} // namespace splonks

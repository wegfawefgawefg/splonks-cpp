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
    float terrain_exposure_min_brightness = 0.50F;
    float terrain_exposure_max_brightness = 1.50F;
    float terrain_exposure_diagonal_weight = 0.50F;
    float terrain_exposure_smoothing = 0.70F;
    float backwall_brightness = 0.85F;
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
    bool ui_settings_visible = false;
    bool post_fx_settings_visible = false;
    bool lighting_settings_visible = false;
    bool graphics_settings_visible = false;
    bool camera_settings_visible = false;

    static DebugUiSettings New();
};

struct Settings {
    SettingsMode mode = SettingsMode::Main;
    VideoSettings video;
    AudioSettings audio;
    ControlsSettings controls;
    UiSettings ui;
    PostProcessSettings post_process;
    DebugUiSettings debug_ui;

    static Settings New();
};

constexpr float kKeyDebounceInterval = 0.2F;

Settings LoadSettings();
bool SaveSettings(const Settings& settings);

} // namespace splonks

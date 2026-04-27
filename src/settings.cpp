#include "settings.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

namespace splonks {

namespace {

std::filesystem::path GetSettingsPath() {
    return std::filesystem::current_path() / "data" / "settings.cfg";
}

bool ParseBool(const std::string& value, bool fallback) {
    if (value == "1" || value == "true" || value == "True") {
        return true;
    }
    if (value == "0" || value == "false" || value == "False") {
        return false;
    }
    return fallback;
}

unsigned int ParseUnsigned(const std::string& value, unsigned int fallback) {
    try {
        return static_cast<unsigned int>(std::stoul(value));
    } catch (...) {
        return fallback;
    }
}

int ParseInt(const std::string& value, int fallback) {
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

float ParseFloat(const std::string& value, float fallback) {
    try {
        return std::stof(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

VideoSettings VideoSettings::New() {
    VideoSettings result;
    result.resolution = UVec2::New(1920, 1080);
    result.fullscreen = true;
    result.vsync = true;
    result.resolution_options = {
        UVec2::New(800, 600),   UVec2::New(1024, 768),  UVec2::New(1280, 720),
        UVec2::New(1280, 1024), UVec2::New(1920, 1080),
    };
    return result;
}

AudioSettings AudioSettings::New() {
    AudioSettings result;
    result.music_volume = 1.0F;
    result.sfx_volume = 1.0F;
    result.pan_half_width_px = 256.0F;
    result.acoustics_enabled = true;
    result.acoustics_occlusion_listener_epsilon_px = 4.0F;
    result.acoustics_reverb_enabled = true;
    result.acoustics_listener_room_weight = 0.65F;
    result.acoustics_direct_min_cutoff_hz = 1200.0F;
    result.acoustics_direct_max_cutoff_hz = 16000.0F;
    result.acoustics_occluded_cutoff_hz = 900.0F;
    result.acoustics_occluded_direct_gain = 0.55F;
    result.acoustics_reverb_send = 0.35F;
    result.acoustics_reverb_delay_ms = 90.0F;
    result.acoustics_reverb_feedback = 0.45F;
    result.acoustics_reverb_min_cutoff_hz = 1200.0F;
    result.acoustics_reverb_max_cutoff_hz = 4200.0F;
    return result;
}

ControlsSettings ControlsSettings::New() {
    ControlsSettings result;
    result.jump = 0;
    result.shoot = 1;
    return result;
}

UiSettings UiSettings::New() {
    UiSettings result;
    result.icon_scale = 1.0F;
    result.status_icon_scale = 1.0F;
    result.tool_slot_scale = 1.0F;
    result.tool_icon_scale = 1.0F;
    return result;
}

PostProcessSettings PostProcessSettings::New() {
    PostProcessSettings result;
    result.effect = PostProcessEffect::Crt;
    result.terrain_lighting = true;
    result.terrain_face_shading = true;
    result.terrain_face_enclosed_stage_bounds = true;
    result.terrain_seam_ao = true;
    result.terrain_exposure_lighting = true;
    result.backwall_lighting = true;
    result.terrain_face_top_highlight = 0.18F;
    result.terrain_face_side_shade = 0.12F;
    result.terrain_face_bottom_shade = 0.20F;
    result.terrain_face_band_size = 0.22F;
    result.terrain_face_gradient_softness = 0.75F;
    result.terrain_face_corner_rounding = 0.80F;
    result.terrain_seam_ao_amount = 0.18F;
    result.terrain_seam_ao_size = 0.20F;
    result.terrain_exposure_amount = 0.12F;
    result.terrain_exposure_remap_enabled = false;
    result.terrain_exposure_input_min = 0.0F;
    result.terrain_exposure_input_max = 1.0F;
    result.terrain_exposure_gamma = 1.0F;
    result.terrain_exposure_output_levels_enabled = true;
    result.terrain_exposure_min_brightness = 0.50F;
    result.terrain_exposure_max_brightness = 1.50F;
    result.terrain_exposure_diagonal_weight = 0.50F;
    result.terrain_exposure_smoothing = 0.70F;
    result.backwall_brightness = 0.85F;
    result.backwall_remap_enabled = false;
    result.backwall_input_min = 0.0F;
    result.backwall_input_max = 1.0F;
    result.backwall_gamma = 1.0F;
    result.backwall_output_levels_enabled = true;
    result.backwall_min_brightness = 0.55F;
    result.backwall_max_brightness = 1.00F;
    result.backwall_smoothing = 0.85F;
    result.crt_scanline_amount = 0.5F;
    result.crt_scanline_edge_start = 0.35F;
    result.crt_scanline_edge_falloff = 0.25F;
    result.crt_scanline_edge_strength = 1.0F;
    result.crt_zoom = 1.0F;
    result.crt_warp_amount = 0.05F;
    result.crt_vignette_amount = 0.5F;
    result.crt_vignette_intensity = 0.3F;
    result.crt_grille_amount = 0.05F;
    result.crt_brightness_boost = 1.2F;
    return result;
}

DebugUiSettings DebugUiSettings::New() {
    DebugUiSettings result;
    result.menu_visible = true;
    result.playback_visible = true;
    result.level_visible = true;
    result.entities_visible = true;
    result.entity_annotations_visible = false;
    result.shake_brush_visible = false;
    result.audio_brush_visible = false;
    result.audio_settings_visible = false;
    result.ui_settings_visible = false;
    result.post_fx_settings_visible = false;
    result.lighting_settings_visible = false;
    result.graphics_settings_visible = false;
    result.camera_settings_visible = false;
    result.performance_settings_visible = false;
    result.player_tuning_visible = false;
    result.entity_swap_type = 1;
    result.default_spawn_type = 1;
    result.default_spawn_enabled = false;
    result.entity_swap_fresh = true;
    result.entity_swap_keep_passives = false;
    result.entity_swap_keep_money = false;
    result.entity_swap_keep_health = false;
    result.entity_swap_keep_tools = false;
    return result;
}

Settings Settings::New() {
    Settings result;
    result.mode = SettingsMode::Main;
    result.video = VideoSettings::New();
    result.audio = AudioSettings::New();
    result.controls = ControlsSettings::New();
    result.ui = UiSettings::New();
    result.post_process = PostProcessSettings::New();
    result.debug_ui = DebugUiSettings::New();
    result.player_tuning = PlayerTuningState{};
    return result;
}

Settings LoadSettings() {
    Settings settings = Settings::New();

    const std::filesystem::path settings_path = GetSettingsPath();
    std::ifstream input(settings_path);
    if (!input.is_open()) {
        return settings;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);

        if (key == "video.resolution_w") {
            settings.video.resolution.x = ParseUnsigned(value, settings.video.resolution.x);
        } else if (key == "video.resolution_h") {
            settings.video.resolution.y = ParseUnsigned(value, settings.video.resolution.y);
        } else if (key == "video.fullscreen") {
            settings.video.fullscreen = ParseBool(value, settings.video.fullscreen);
        } else if (key == "video.vsync") {
            settings.video.vsync = ParseBool(value, settings.video.vsync);
        } else if (key == "audio.music_volume") {
            settings.audio.music_volume = ParseFloat(value, settings.audio.music_volume);
        } else if (key == "audio.sfx_volume") {
            settings.audio.sfx_volume = ParseFloat(value, settings.audio.sfx_volume);
        } else if (key == "audio.pan_half_width_px") {
            settings.audio.pan_half_width_px = ParseFloat(value, settings.audio.pan_half_width_px);
        } else if (key == "audio.acoustics_enabled") {
            settings.audio.acoustics_enabled =
                ParseBool(value, settings.audio.acoustics_enabled);
        } else if (key == "audio.acoustics_occlusion_listener_epsilon_px") {
            settings.audio.acoustics_occlusion_listener_epsilon_px =
                ParseFloat(value, settings.audio.acoustics_occlusion_listener_epsilon_px);
        } else if (key == "audio.acoustics_reverb_enabled") {
            settings.audio.acoustics_reverb_enabled =
                ParseBool(value, settings.audio.acoustics_reverb_enabled);
        } else if (key == "audio.acoustics_listener_room_weight") {
            settings.audio.acoustics_listener_room_weight =
                ParseFloat(value, settings.audio.acoustics_listener_room_weight);
        } else if (key == "audio.acoustics_direct_min_cutoff_hz") {
            settings.audio.acoustics_direct_min_cutoff_hz =
                ParseFloat(value, settings.audio.acoustics_direct_min_cutoff_hz);
        } else if (key == "audio.acoustics_direct_max_cutoff_hz") {
            settings.audio.acoustics_direct_max_cutoff_hz =
                ParseFloat(value, settings.audio.acoustics_direct_max_cutoff_hz);
        } else if (key == "audio.acoustics_occluded_cutoff_hz") {
            settings.audio.acoustics_occluded_cutoff_hz =
                ParseFloat(value, settings.audio.acoustics_occluded_cutoff_hz);
        } else if (key == "audio.acoustics_occluded_direct_gain") {
            settings.audio.acoustics_occluded_direct_gain =
                ParseFloat(value, settings.audio.acoustics_occluded_direct_gain);
        } else if (key == "audio.acoustics_reverb_send") {
            settings.audio.acoustics_reverb_send =
                ParseFloat(value, settings.audio.acoustics_reverb_send);
        } else if (key == "audio.acoustics_reverb_delay_ms") {
            settings.audio.acoustics_reverb_delay_ms =
                ParseFloat(value, settings.audio.acoustics_reverb_delay_ms);
        } else if (key == "audio.acoustics_reverb_feedback") {
            settings.audio.acoustics_reverb_feedback =
                ParseFloat(value, settings.audio.acoustics_reverb_feedback);
        } else if (key == "audio.acoustics_reverb_min_cutoff_hz") {
            settings.audio.acoustics_reverb_min_cutoff_hz =
                ParseFloat(value, settings.audio.acoustics_reverb_min_cutoff_hz);
        } else if (key == "audio.acoustics_reverb_max_cutoff_hz") {
            settings.audio.acoustics_reverb_max_cutoff_hz =
                ParseFloat(value, settings.audio.acoustics_reverb_max_cutoff_hz);
        } else if (key == "controls.jump") {
            settings.controls.jump = ParseUnsigned(value, settings.controls.jump);
        } else if (key == "controls.shoot") {
            settings.controls.shoot = ParseUnsigned(value, settings.controls.shoot);
        } else if (key == "ui.icon_scale") {
            settings.ui.icon_scale = ParseFloat(value, settings.ui.icon_scale);
        } else if (key == "ui.status_icon_scale") {
            settings.ui.status_icon_scale = ParseFloat(value, settings.ui.status_icon_scale);
        } else if (key == "ui.tool_slot_scale") {
            settings.ui.tool_slot_scale = ParseFloat(value, settings.ui.tool_slot_scale);
        } else if (key == "ui.tool_icon_scale") {
            settings.ui.tool_icon_scale = ParseFloat(value, settings.ui.tool_icon_scale);
        } else if (key == "post_process.effect") {
            settings.post_process.effect =
                static_cast<PostProcessEffect>(ParseUnsigned(
                    value,
                    static_cast<unsigned int>(settings.post_process.effect)
                ));
        } else if (key == "post_process.terrain_lighting") {
            settings.post_process.terrain_lighting =
                ParseBool(value, settings.post_process.terrain_lighting);
        } else if (key == "post_process.terrain_face_shading") {
            settings.post_process.terrain_face_shading =
                ParseBool(value, settings.post_process.terrain_face_shading);
        } else if (key == "post_process.terrain_face_enclosed_stage_bounds") {
            settings.post_process.terrain_face_enclosed_stage_bounds =
                ParseBool(value, settings.post_process.terrain_face_enclosed_stage_bounds);
        } else if (key == "post_process.terrain_seam_ao") {
            settings.post_process.terrain_seam_ao =
                ParseBool(value, settings.post_process.terrain_seam_ao);
        } else if (key == "post_process.terrain_exposure_lighting") {
            settings.post_process.terrain_exposure_lighting =
                ParseBool(value, settings.post_process.terrain_exposure_lighting);
        } else if (key == "post_process.backwall_lighting") {
            settings.post_process.backwall_lighting =
                ParseBool(value, settings.post_process.backwall_lighting);
        } else if (key == "post_process.terrain_face_top_highlight") {
            settings.post_process.terrain_face_top_highlight =
                ParseFloat(value, settings.post_process.terrain_face_top_highlight);
        } else if (key == "post_process.terrain_face_side_shade") {
            settings.post_process.terrain_face_side_shade =
                ParseFloat(value, settings.post_process.terrain_face_side_shade);
        } else if (key == "post_process.terrain_face_bottom_shade") {
            settings.post_process.terrain_face_bottom_shade =
                ParseFloat(value, settings.post_process.terrain_face_bottom_shade);
        } else if (key == "post_process.terrain_face_band_size") {
            settings.post_process.terrain_face_band_size =
                ParseFloat(value, settings.post_process.terrain_face_band_size);
        } else if (key == "post_process.terrain_face_gradient_softness") {
            settings.post_process.terrain_face_gradient_softness =
                ParseFloat(value, settings.post_process.terrain_face_gradient_softness);
        } else if (key == "post_process.terrain_face_corner_rounding") {
            settings.post_process.terrain_face_corner_rounding =
                ParseFloat(value, settings.post_process.terrain_face_corner_rounding);
        } else if (key == "post_process.terrain_seam_ao_amount") {
            settings.post_process.terrain_seam_ao_amount =
                ParseFloat(value, settings.post_process.terrain_seam_ao_amount);
        } else if (key == "post_process.terrain_seam_ao_size") {
            settings.post_process.terrain_seam_ao_size =
                ParseFloat(value, settings.post_process.terrain_seam_ao_size);
        } else if (key == "post_process.terrain_exposure_amount") {
            settings.post_process.terrain_exposure_amount =
                ParseFloat(value, settings.post_process.terrain_exposure_amount);
        } else if (key == "post_process.terrain_exposure_remap_enabled") {
            settings.post_process.terrain_exposure_remap_enabled =
                ParseBool(value, settings.post_process.terrain_exposure_remap_enabled);
        } else if (key == "post_process.terrain_exposure_input_min") {
            settings.post_process.terrain_exposure_input_min =
                ParseFloat(value, settings.post_process.terrain_exposure_input_min);
        } else if (key == "post_process.terrain_exposure_input_max") {
            settings.post_process.terrain_exposure_input_max =
                ParseFloat(value, settings.post_process.terrain_exposure_input_max);
        } else if (key == "post_process.terrain_exposure_gamma") {
            settings.post_process.terrain_exposure_gamma =
                ParseFloat(value, settings.post_process.terrain_exposure_gamma);
        } else if (key == "post_process.terrain_exposure_output_levels_enabled") {
            settings.post_process.terrain_exposure_output_levels_enabled =
                ParseBool(value, settings.post_process.terrain_exposure_output_levels_enabled);
        } else if (key == "post_process.terrain_exposure_min_brightness") {
            settings.post_process.terrain_exposure_min_brightness =
                ParseFloat(value, settings.post_process.terrain_exposure_min_brightness);
        } else if (key == "post_process.terrain_exposure_max_brightness") {
            settings.post_process.terrain_exposure_max_brightness =
                ParseFloat(value, settings.post_process.terrain_exposure_max_brightness);
        } else if (key == "post_process.terrain_exposure_diagonal_weight") {
            settings.post_process.terrain_exposure_diagonal_weight =
                ParseFloat(value, settings.post_process.terrain_exposure_diagonal_weight);
        } else if (key == "post_process.terrain_exposure_smoothing") {
            settings.post_process.terrain_exposure_smoothing =
                ParseFloat(value, settings.post_process.terrain_exposure_smoothing);
        } else if (key == "post_process.backwall_brightness") {
            settings.post_process.backwall_brightness =
                ParseFloat(value, settings.post_process.backwall_brightness);
        } else if (key == "post_process.backwall_remap_enabled") {
            settings.post_process.backwall_remap_enabled =
                ParseBool(value, settings.post_process.backwall_remap_enabled);
        } else if (key == "post_process.backwall_input_min") {
            settings.post_process.backwall_input_min =
                ParseFloat(value, settings.post_process.backwall_input_min);
        } else if (key == "post_process.backwall_input_max") {
            settings.post_process.backwall_input_max =
                ParseFloat(value, settings.post_process.backwall_input_max);
        } else if (key == "post_process.backwall_gamma") {
            settings.post_process.backwall_gamma =
                ParseFloat(value, settings.post_process.backwall_gamma);
        } else if (key == "post_process.backwall_output_levels_enabled") {
            settings.post_process.backwall_output_levels_enabled =
                ParseBool(value, settings.post_process.backwall_output_levels_enabled);
        } else if (key == "post_process.backwall_min_brightness") {
            settings.post_process.backwall_min_brightness =
                ParseFloat(value, settings.post_process.backwall_min_brightness);
        } else if (key == "post_process.backwall_max_brightness") {
            settings.post_process.backwall_max_brightness =
                ParseFloat(value, settings.post_process.backwall_max_brightness);
        } else if (key == "post_process.backwall_smoothing") {
            settings.post_process.backwall_smoothing =
                ParseFloat(value, settings.post_process.backwall_smoothing);
        } else if (key == "post_process.crt_scanline_amount") {
            settings.post_process.crt_scanline_amount =
                ParseFloat(value, settings.post_process.crt_scanline_amount);
        } else if (key == "post_process.crt_scanline_edge_start") {
            settings.post_process.crt_scanline_edge_start =
                ParseFloat(value, settings.post_process.crt_scanline_edge_start);
        } else if (key == "post_process.crt_scanline_edge_falloff") {
            settings.post_process.crt_scanline_edge_falloff =
                ParseFloat(value, settings.post_process.crt_scanline_edge_falloff);
        } else if (key == "post_process.crt_scanline_edge_strength") {
            settings.post_process.crt_scanline_edge_strength =
                ParseFloat(value, settings.post_process.crt_scanline_edge_strength);
        } else if (key == "post_process.crt_zoom") {
            settings.post_process.crt_zoom =
                ParseFloat(value, settings.post_process.crt_zoom);
        } else if (key == "post_process.crt_warp_amount") {
            settings.post_process.crt_warp_amount =
                ParseFloat(value, settings.post_process.crt_warp_amount);
        } else if (key == "post_process.crt_vignette_amount") {
            settings.post_process.crt_vignette_amount =
                ParseFloat(value, settings.post_process.crt_vignette_amount);
        } else if (key == "post_process.crt_vignette_intensity") {
            settings.post_process.crt_vignette_intensity =
                ParseFloat(value, settings.post_process.crt_vignette_intensity);
        } else if (key == "post_process.crt_grille_amount") {
            settings.post_process.crt_grille_amount =
                ParseFloat(value, settings.post_process.crt_grille_amount);
        } else if (key == "post_process.crt_brightness_boost") {
            settings.post_process.crt_brightness_boost =
                ParseFloat(value, settings.post_process.crt_brightness_boost);
        } else if (key == "debug_ui.menu_visible") {
            settings.debug_ui.menu_visible = ParseBool(value, settings.debug_ui.menu_visible);
        } else if (key == "debug_ui.playback_visible") {
            settings.debug_ui.playback_visible = ParseBool(value, settings.debug_ui.playback_visible);
        } else if (key == "debug_ui.level_visible") {
            settings.debug_ui.level_visible = ParseBool(value, settings.debug_ui.level_visible);
        } else if (key == "debug_ui.entities_visible") {
            settings.debug_ui.entities_visible = ParseBool(value, settings.debug_ui.entities_visible);
        } else if (key == "debug_ui.entity_annotations_visible") {
            settings.debug_ui.entity_annotations_visible =
                ParseBool(value, settings.debug_ui.entity_annotations_visible);
        } else if (key == "debug_ui.shake_brush_visible") {
            settings.debug_ui.shake_brush_visible =
                ParseBool(value, settings.debug_ui.shake_brush_visible);
        } else if (key == "debug_ui.audio_brush_visible") {
            settings.debug_ui.audio_brush_visible =
                ParseBool(value, settings.debug_ui.audio_brush_visible);
        } else if (key == "debug_ui.audio_settings_visible") {
            settings.debug_ui.audio_settings_visible =
                ParseBool(value, settings.debug_ui.audio_settings_visible);
        } else if (key == "debug_ui.ui_settings_visible") {
            settings.debug_ui.ui_settings_visible =
                ParseBool(value, settings.debug_ui.ui_settings_visible);
        } else if (key == "debug_ui.post_fx_settings_visible") {
            settings.debug_ui.post_fx_settings_visible =
                ParseBool(value, settings.debug_ui.post_fx_settings_visible);
        } else if (key == "debug_ui.lighting_settings_visible") {
            settings.debug_ui.lighting_settings_visible =
                ParseBool(value, settings.debug_ui.lighting_settings_visible);
        } else if (key == "debug_ui.graphics_settings_visible") {
            settings.debug_ui.graphics_settings_visible =
                ParseBool(value, settings.debug_ui.graphics_settings_visible);
        } else if (key == "debug_ui.camera_settings_visible") {
            settings.debug_ui.camera_settings_visible =
                ParseBool(value, settings.debug_ui.camera_settings_visible);
        } else if (key == "debug_ui.performance_settings_visible") {
            settings.debug_ui.performance_settings_visible =
                ParseBool(value, settings.debug_ui.performance_settings_visible);
        } else if (key == "debug_ui.player_tuning_visible") {
            settings.debug_ui.player_tuning_visible =
                ParseBool(value, settings.debug_ui.player_tuning_visible);
        } else if (key == "debug_ui.entity_swap_type") {
            settings.debug_ui.entity_swap_type =
                ParseUnsigned(value, settings.debug_ui.entity_swap_type);
        } else if (key == "debug_ui.default_spawn_type") {
            settings.debug_ui.default_spawn_type =
                ParseUnsigned(value, settings.debug_ui.default_spawn_type);
        } else if (key == "debug_ui.default_spawn_enabled") {
            settings.debug_ui.default_spawn_enabled =
                ParseBool(value, settings.debug_ui.default_spawn_enabled);
        } else if (key == "debug_ui.entity_swap_fresh") {
            settings.debug_ui.entity_swap_fresh =
                ParseBool(value, settings.debug_ui.entity_swap_fresh);
        } else if (key == "debug_ui.entity_swap_keep_passives") {
            settings.debug_ui.entity_swap_keep_passives =
                ParseBool(value, settings.debug_ui.entity_swap_keep_passives);
        } else if (key == "debug_ui.entity_swap_keep_money") {
            settings.debug_ui.entity_swap_keep_money =
                ParseBool(value, settings.debug_ui.entity_swap_keep_money);
        } else if (key == "debug_ui.entity_swap_keep_health") {
            settings.debug_ui.entity_swap_keep_health =
                ParseBool(value, settings.debug_ui.entity_swap_keep_health);
        } else if (key == "debug_ui.entity_swap_keep_tools") {
            settings.debug_ui.entity_swap_keep_tools =
                ParseBool(value, settings.debug_ui.entity_swap_keep_tools);
        } else if (key == "player_tuning.gravity_scale") {
            settings.player_tuning.gravity_scale =
                ParseFloat(value, settings.player_tuning.gravity_scale);
        } else if (key == "player_tuning.max_fall_speed") {
            settings.player_tuning.max_fall_speed =
                ParseFloat(value, settings.player_tuning.max_fall_speed);
        } else if (key == "player_tuning.jump_impulse") {
            settings.player_tuning.jump_impulse =
                ParseFloat(value, settings.player_tuning.jump_impulse);
        } else if (key == "player_tuning.spring_shoes_jump_impulse_bonus") {
            settings.player_tuning.spring_shoes_jump_impulse_bonus =
                ParseFloat(value, settings.player_tuning.spring_shoes_jump_impulse_bonus);
        } else if (key == "player_tuning.jump_hold_frames") {
            settings.player_tuning.jump_hold_frames =
                ParseInt(value, settings.player_tuning.jump_hold_frames);
        } else if (key == "player_tuning.coyote_frames") {
            settings.player_tuning.coyote_frames =
                ParseInt(value, settings.player_tuning.coyote_frames);
        } else if (key == "player_tuning.jump_delay_frames") {
            settings.player_tuning.jump_delay_frames =
                ParseInt(value, settings.player_tuning.jump_delay_frames);
        } else if (key == "player_tuning.fall_damage_light_frames") {
            settings.player_tuning.fall_damage_light_frames =
                ParseInt(value, settings.player_tuning.fall_damage_light_frames);
        } else if (key == "player_tuning.fall_damage_medium_frames") {
            settings.player_tuning.fall_damage_medium_frames =
                ParseInt(value, settings.player_tuning.fall_damage_medium_frames);
        } else if (key == "player_tuning.fall_damage_heavy_frames") {
            settings.player_tuning.fall_damage_heavy_frames =
                ParseInt(value, settings.player_tuning.fall_damage_heavy_frames);
        } else if (key == "player_tuning.walk_speed") {
            settings.player_tuning.walk_speed =
                ParseFloat(value, settings.player_tuning.walk_speed);
        } else if (key == "player_tuning.run_speed") {
            settings.player_tuning.run_speed =
                ParseFloat(value, settings.player_tuning.run_speed);
        } else if (key == "player_tuning.move_acc") {
            settings.player_tuning.move_acc =
                ParseFloat(value, settings.player_tuning.move_acc);
        } else if (key == "player_tuning.run_acc") {
            settings.player_tuning.run_acc =
                ParseFloat(value, settings.player_tuning.run_acc);
        } else if (key == "player_tuning.ground_friction_scale") {
            settings.player_tuning.ground_friction_scale =
                ParseFloat(value, settings.player_tuning.ground_friction_scale);
        } else if (key == "player_tuning.air_friction") {
            settings.player_tuning.air_friction =
                ParseFloat(value, settings.player_tuning.air_friction);
        } else if (key == "player_tuning.climb_speed") {
            settings.player_tuning.climb_speed =
                ParseFloat(value, settings.player_tuning.climb_speed);
        } else if (key == "player_tuning.climb_depart_horizontal_speed") {
            settings.player_tuning.climb_depart_horizontal_speed =
                ParseFloat(value, settings.player_tuning.climb_depart_horizontal_speed);
        } else if (key == "player_tuning.climb_probe_bias_pixels") {
            settings.player_tuning.climb_probe_bias_pixels =
                ParseFloat(value, settings.player_tuning.climb_probe_bias_pixels);
        } else if (key == "player_tuning.climb_probe_x_scale") {
            settings.player_tuning.climb_probe_x_scale =
                ParseFloat(value, settings.player_tuning.climb_probe_x_scale);
        } else if (key == "player_tuning.climb_required_probe_hits") {
            settings.player_tuning.climb_required_probe_hits =
                ParseInt(value, settings.player_tuning.climb_required_probe_hits);
        } else if (key == "player_tuning.climb_detach_cooldown") {
            settings.player_tuning.climb_detach_cooldown =
                ParseInt(value, settings.player_tuning.climb_detach_cooldown);
        } else if (key == "player_tuning.hang_drop_cooldown") {
            settings.player_tuning.hang_drop_cooldown =
                ParseInt(value, settings.player_tuning.hang_drop_cooldown);
        } else if (key == "player_tuning.glove_hang_drop_cooldown") {
            settings.player_tuning.glove_hang_drop_cooldown =
                ParseInt(value, settings.player_tuning.glove_hang_drop_cooldown);
        } else if (key == "player_tuning.hang_wall_release_cooldown") {
            settings.player_tuning.hang_wall_release_cooldown =
                ParseInt(value, settings.player_tuning.hang_wall_release_cooldown);
        } else if (key == "player_tuning.auto_ledge_grab") {
            settings.player_tuning.auto_ledge_grab =
                ParseBool(value, settings.player_tuning.auto_ledge_grab);
        }
    }

    return settings;
}

bool SaveSettings(const Settings& settings) {
    const std::filesystem::path settings_path = GetSettingsPath();
    std::filesystem::create_directories(settings_path.parent_path());
    std::ofstream output(settings_path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "video.resolution_w=" << settings.video.resolution.x << "\n";
    output << "video.resolution_h=" << settings.video.resolution.y << "\n";
    output << "video.fullscreen=" << (settings.video.fullscreen ? 1 : 0) << "\n";
    output << "video.vsync=" << (settings.video.vsync ? 1 : 0) << "\n";
    output << "audio.music_volume=" << settings.audio.music_volume << "\n";
    output << "audio.sfx_volume=" << settings.audio.sfx_volume << "\n";
    output << "audio.pan_half_width_px=" << settings.audio.pan_half_width_px << "\n";
    output << "audio.acoustics_enabled=" << (settings.audio.acoustics_enabled ? 1 : 0) << "\n";
    output << "audio.acoustics_occlusion_listener_epsilon_px="
           << settings.audio.acoustics_occlusion_listener_epsilon_px << "\n";
    output << "audio.acoustics_reverb_enabled="
           << (settings.audio.acoustics_reverb_enabled ? 1 : 0) << "\n";
    output << "audio.acoustics_listener_room_weight="
           << settings.audio.acoustics_listener_room_weight << "\n";
    output << "audio.acoustics_direct_min_cutoff_hz="
           << settings.audio.acoustics_direct_min_cutoff_hz << "\n";
    output << "audio.acoustics_direct_max_cutoff_hz="
           << settings.audio.acoustics_direct_max_cutoff_hz << "\n";
    output << "audio.acoustics_occluded_cutoff_hz="
           << settings.audio.acoustics_occluded_cutoff_hz << "\n";
    output << "audio.acoustics_occluded_direct_gain="
           << settings.audio.acoustics_occluded_direct_gain << "\n";
    output << "audio.acoustics_reverb_send="
           << settings.audio.acoustics_reverb_send << "\n";
    output << "audio.acoustics_reverb_delay_ms="
           << settings.audio.acoustics_reverb_delay_ms << "\n";
    output << "audio.acoustics_reverb_feedback="
           << settings.audio.acoustics_reverb_feedback << "\n";
    output << "audio.acoustics_reverb_min_cutoff_hz="
           << settings.audio.acoustics_reverb_min_cutoff_hz << "\n";
    output << "audio.acoustics_reverb_max_cutoff_hz="
           << settings.audio.acoustics_reverb_max_cutoff_hz << "\n";
    output << "controls.jump=" << settings.controls.jump << "\n";
    output << "controls.shoot=" << settings.controls.shoot << "\n";
    output << "ui.icon_scale=" << settings.ui.icon_scale << "\n";
    output << "ui.status_icon_scale=" << settings.ui.status_icon_scale << "\n";
    output << "ui.tool_slot_scale=" << settings.ui.tool_slot_scale << "\n";
    output << "ui.tool_icon_scale=" << settings.ui.tool_icon_scale << "\n";
    output << "post_process.effect=" << static_cast<unsigned int>(settings.post_process.effect) << "\n";
    output << "post_process.terrain_lighting="
           << (settings.post_process.terrain_lighting ? 1 : 0) << "\n";
    output << "post_process.terrain_face_shading="
           << (settings.post_process.terrain_face_shading ? 1 : 0) << "\n";
    output << "post_process.terrain_face_enclosed_stage_bounds="
           << (settings.post_process.terrain_face_enclosed_stage_bounds ? 1 : 0) << "\n";
    output << "post_process.terrain_seam_ao="
           << (settings.post_process.terrain_seam_ao ? 1 : 0) << "\n";
    output << "post_process.terrain_exposure_lighting="
           << (settings.post_process.terrain_exposure_lighting ? 1 : 0) << "\n";
    output << "post_process.backwall_lighting="
           << (settings.post_process.backwall_lighting ? 1 : 0) << "\n";
    output << "post_process.terrain_face_top_highlight="
           << settings.post_process.terrain_face_top_highlight << "\n";
    output << "post_process.terrain_face_side_shade="
           << settings.post_process.terrain_face_side_shade << "\n";
    output << "post_process.terrain_face_bottom_shade="
           << settings.post_process.terrain_face_bottom_shade << "\n";
    output << "post_process.terrain_face_band_size="
           << settings.post_process.terrain_face_band_size << "\n";
    output << "post_process.terrain_face_gradient_softness="
           << settings.post_process.terrain_face_gradient_softness << "\n";
    output << "post_process.terrain_face_corner_rounding="
           << settings.post_process.terrain_face_corner_rounding << "\n";
    output << "post_process.terrain_seam_ao_amount="
           << settings.post_process.terrain_seam_ao_amount << "\n";
    output << "post_process.terrain_seam_ao_size="
           << settings.post_process.terrain_seam_ao_size << "\n";
    output << "post_process.terrain_exposure_amount="
           << settings.post_process.terrain_exposure_amount << "\n";
    output << "post_process.terrain_exposure_remap_enabled="
           << (settings.post_process.terrain_exposure_remap_enabled ? 1 : 0) << "\n";
    output << "post_process.terrain_exposure_input_min="
           << settings.post_process.terrain_exposure_input_min << "\n";
    output << "post_process.terrain_exposure_input_max="
           << settings.post_process.terrain_exposure_input_max << "\n";
    output << "post_process.terrain_exposure_gamma="
           << settings.post_process.terrain_exposure_gamma << "\n";
    output << "post_process.terrain_exposure_output_levels_enabled="
           << (settings.post_process.terrain_exposure_output_levels_enabled ? 1 : 0) << "\n";
    output << "post_process.terrain_exposure_min_brightness="
           << settings.post_process.terrain_exposure_min_brightness << "\n";
    output << "post_process.terrain_exposure_max_brightness="
           << settings.post_process.terrain_exposure_max_brightness << "\n";
    output << "post_process.terrain_exposure_diagonal_weight="
           << settings.post_process.terrain_exposure_diagonal_weight << "\n";
    output << "post_process.terrain_exposure_smoothing="
           << settings.post_process.terrain_exposure_smoothing << "\n";
    output << "post_process.backwall_brightness="
           << settings.post_process.backwall_brightness << "\n";
    output << "post_process.backwall_remap_enabled="
           << (settings.post_process.backwall_remap_enabled ? 1 : 0) << "\n";
    output << "post_process.backwall_input_min="
           << settings.post_process.backwall_input_min << "\n";
    output << "post_process.backwall_input_max="
           << settings.post_process.backwall_input_max << "\n";
    output << "post_process.backwall_gamma="
           << settings.post_process.backwall_gamma << "\n";
    output << "post_process.backwall_output_levels_enabled="
           << (settings.post_process.backwall_output_levels_enabled ? 1 : 0) << "\n";
    output << "post_process.backwall_min_brightness="
           << settings.post_process.backwall_min_brightness << "\n";
    output << "post_process.backwall_max_brightness="
           << settings.post_process.backwall_max_brightness << "\n";
    output << "post_process.backwall_smoothing="
           << settings.post_process.backwall_smoothing << "\n";
    output << "post_process.crt_scanline_amount=" << settings.post_process.crt_scanline_amount << "\n";
    output << "post_process.crt_scanline_edge_start="
           << settings.post_process.crt_scanline_edge_start << "\n";
    output << "post_process.crt_scanline_edge_falloff="
           << settings.post_process.crt_scanline_edge_falloff << "\n";
    output << "post_process.crt_scanline_edge_strength="
           << settings.post_process.crt_scanline_edge_strength << "\n";
    output << "post_process.crt_zoom=" << settings.post_process.crt_zoom << "\n";
    output << "post_process.crt_warp_amount=" << settings.post_process.crt_warp_amount << "\n";
    output << "post_process.crt_vignette_amount=" << settings.post_process.crt_vignette_amount << "\n";
    output << "post_process.crt_vignette_intensity=" << settings.post_process.crt_vignette_intensity << "\n";
    output << "post_process.crt_grille_amount=" << settings.post_process.crt_grille_amount << "\n";
    output << "post_process.crt_brightness_boost=" << settings.post_process.crt_brightness_boost << "\n";
    output << "debug_ui.menu_visible=" << (settings.debug_ui.menu_visible ? 1 : 0) << "\n";
    output << "debug_ui.playback_visible=" << (settings.debug_ui.playback_visible ? 1 : 0) << "\n";
    output << "debug_ui.level_visible=" << (settings.debug_ui.level_visible ? 1 : 0) << "\n";
    output << "debug_ui.entities_visible=" << (settings.debug_ui.entities_visible ? 1 : 0) << "\n";
    output << "debug_ui.entity_annotations_visible="
           << (settings.debug_ui.entity_annotations_visible ? 1 : 0) << "\n";
    output << "debug_ui.shake_brush_visible="
           << (settings.debug_ui.shake_brush_visible ? 1 : 0) << "\n";
    output << "debug_ui.audio_brush_visible="
           << (settings.debug_ui.audio_brush_visible ? 1 : 0) << "\n";
    output << "debug_ui.audio_settings_visible="
           << (settings.debug_ui.audio_settings_visible ? 1 : 0) << "\n";
    output << "debug_ui.ui_settings_visible="
           << (settings.debug_ui.ui_settings_visible ? 1 : 0) << "\n";
    output << "debug_ui.post_fx_settings_visible="
           << (settings.debug_ui.post_fx_settings_visible ? 1 : 0) << "\n";
    output << "debug_ui.lighting_settings_visible="
           << (settings.debug_ui.lighting_settings_visible ? 1 : 0) << "\n";
    output << "debug_ui.graphics_settings_visible="
           << (settings.debug_ui.graphics_settings_visible ? 1 : 0) << "\n";
    output << "debug_ui.camera_settings_visible="
           << (settings.debug_ui.camera_settings_visible ? 1 : 0) << "\n";
    output << "debug_ui.performance_settings_visible="
           << (settings.debug_ui.performance_settings_visible ? 1 : 0) << "\n";
    output << "debug_ui.player_tuning_visible="
           << (settings.debug_ui.player_tuning_visible ? 1 : 0) << "\n";
    output << "debug_ui.entity_swap_type=" << settings.debug_ui.entity_swap_type << "\n";
    output << "debug_ui.default_spawn_type=" << settings.debug_ui.default_spawn_type << "\n";
    output << "debug_ui.default_spawn_enabled="
           << (settings.debug_ui.default_spawn_enabled ? 1 : 0) << "\n";
    output << "debug_ui.entity_swap_fresh="
           << (settings.debug_ui.entity_swap_fresh ? 1 : 0) << "\n";
    output << "debug_ui.entity_swap_keep_passives="
           << (settings.debug_ui.entity_swap_keep_passives ? 1 : 0) << "\n";
    output << "debug_ui.entity_swap_keep_money="
           << (settings.debug_ui.entity_swap_keep_money ? 1 : 0) << "\n";
    output << "debug_ui.entity_swap_keep_health="
           << (settings.debug_ui.entity_swap_keep_health ? 1 : 0) << "\n";
    output << "debug_ui.entity_swap_keep_tools="
           << (settings.debug_ui.entity_swap_keep_tools ? 1 : 0) << "\n";
    output << "player_tuning.gravity_scale=" << settings.player_tuning.gravity_scale << "\n";
    output << "player_tuning.max_fall_speed=" << settings.player_tuning.max_fall_speed << "\n";
    output << "player_tuning.jump_impulse=" << settings.player_tuning.jump_impulse << "\n";
    output << "player_tuning.spring_shoes_jump_impulse_bonus="
           << settings.player_tuning.spring_shoes_jump_impulse_bonus << "\n";
    output << "player_tuning.jump_hold_frames=" << settings.player_tuning.jump_hold_frames << "\n";
    output << "player_tuning.coyote_frames=" << settings.player_tuning.coyote_frames << "\n";
    output << "player_tuning.jump_delay_frames=" << settings.player_tuning.jump_delay_frames << "\n";
    output << "player_tuning.fall_damage_light_frames="
           << settings.player_tuning.fall_damage_light_frames << "\n";
    output << "player_tuning.fall_damage_medium_frames="
           << settings.player_tuning.fall_damage_medium_frames << "\n";
    output << "player_tuning.fall_damage_heavy_frames="
           << settings.player_tuning.fall_damage_heavy_frames << "\n";
    output << "player_tuning.walk_speed=" << settings.player_tuning.walk_speed << "\n";
    output << "player_tuning.run_speed=" << settings.player_tuning.run_speed << "\n";
    output << "player_tuning.move_acc=" << settings.player_tuning.move_acc << "\n";
    output << "player_tuning.run_acc=" << settings.player_tuning.run_acc << "\n";
    output << "player_tuning.ground_friction_scale="
           << settings.player_tuning.ground_friction_scale << "\n";
    output << "player_tuning.air_friction=" << settings.player_tuning.air_friction << "\n";
    output << "player_tuning.climb_speed=" << settings.player_tuning.climb_speed << "\n";
    output << "player_tuning.climb_depart_horizontal_speed="
           << settings.player_tuning.climb_depart_horizontal_speed << "\n";
    output << "player_tuning.climb_probe_bias_pixels="
           << settings.player_tuning.climb_probe_bias_pixels << "\n";
    output << "player_tuning.climb_probe_x_scale="
           << settings.player_tuning.climb_probe_x_scale << "\n";
    output << "player_tuning.climb_required_probe_hits="
           << settings.player_tuning.climb_required_probe_hits << "\n";
    output << "player_tuning.climb_detach_cooldown="
           << settings.player_tuning.climb_detach_cooldown << "\n";
    output << "player_tuning.hang_drop_cooldown=" << settings.player_tuning.hang_drop_cooldown << "\n";
    output << "player_tuning.glove_hang_drop_cooldown="
           << settings.player_tuning.glove_hang_drop_cooldown << "\n";
    output << "player_tuning.hang_wall_release_cooldown="
           << settings.player_tuning.hang_wall_release_cooldown << "\n";
    output << "player_tuning.auto_ledge_grab="
           << (settings.player_tuning.auto_ledge_grab ? 1 : 0) << "\n";
    return output.good();
}

} // namespace splonks

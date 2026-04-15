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
    result.terrain_exposure_min_brightness = 0.50F;
    result.terrain_exposure_max_brightness = 1.50F;
    result.terrain_exposure_diagonal_weight = 0.50F;
    result.terrain_exposure_smoothing = 0.70F;
    result.backwall_brightness = 0.85F;
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
    result.ui_settings_visible = false;
    result.post_fx_settings_visible = false;
    result.lighting_settings_visible = false;
    result.graphics_settings_visible = false;
    result.camera_settings_visible = false;
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
    return output.good();
}

} // namespace splonks

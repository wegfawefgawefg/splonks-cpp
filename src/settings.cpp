#include "settings.hpp"

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
    result.vsync = false;
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

DebugUiSettings DebugUiSettings::New() {
    DebugUiSettings result;
    result.menu_visible = true;
    result.playback_visible = true;
    result.level_visible = true;
    result.entities_visible = true;
    result.entity_annotations_visible = false;
    result.ui_settings_visible = false;
    return result;
}

Settings Settings::New() {
    Settings result;
    result.mode = SettingsMode::Main;
    result.video = VideoSettings::New();
    result.audio = AudioSettings::New();
    result.controls = ControlsSettings::New();
    result.ui = UiSettings::New();
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
    output << "debug_ui.menu_visible=" << (settings.debug_ui.menu_visible ? 1 : 0) << "\n";
    output << "debug_ui.playback_visible=" << (settings.debug_ui.playback_visible ? 1 : 0) << "\n";
    output << "debug_ui.level_visible=" << (settings.debug_ui.level_visible ? 1 : 0) << "\n";
    output << "debug_ui.entities_visible=" << (settings.debug_ui.entities_visible ? 1 : 0) << "\n";
    output << "debug_ui.entity_annotations_visible="
           << (settings.debug_ui.entity_annotations_visible ? 1 : 0) << "\n";
    output << "debug_ui.ui_settings_visible="
           << (settings.debug_ui.ui_settings_visible ? 1 : 0) << "\n";
    return output.good();
}

} // namespace splonks

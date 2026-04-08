#pragma once

#include "math_types.hpp"

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

struct DebugUiSettings {
    bool menu_visible = true;
    bool playback_visible = true;
    bool level_visible = true;
    bool entities_visible = true;
    bool entity_annotations_visible = false;
    bool ui_settings_visible = false;

    static DebugUiSettings New();
};

struct Settings {
    SettingsMode mode = SettingsMode::Main;
    VideoSettings video;
    AudioSettings audio;
    ControlsSettings controls;
    UiSettings ui;
    DebugUiSettings debug_ui;

    static Settings New();
};

constexpr float kKeyDebounceInterval = 0.2F;

Settings LoadSettings();
bool SaveSettings(const Settings& settings);

} // namespace splonks

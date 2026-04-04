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

struct Settings {
    SettingsMode mode = SettingsMode::Main;
    VideoSettings video;
    AudioSettings audio;
    ControlsSettings controls;

    static Settings New();
};

constexpr float kKeyDebounceInterval = 0.2F;

} // namespace splonks

#include "settings.hpp"

namespace splonks {

VideoSettings VideoSettings::New() {
    VideoSettings result;
    result.resolution = UVec2::New(1280, 720);
    result.fullscreen = false;
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

Settings Settings::New() {
    Settings result;
    result.mode = SettingsMode::Main;
    result.video = VideoSettings::New();
    result.audio = AudioSettings::New();
    result.controls = ControlsSettings::New();
    return result;
}

} // namespace splonks

#include "menu_video.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
#include "settings.hpp"
#include "state.hpp"

namespace splonks {

const std::array<UVec2, 10> kResolutions = {
    UVec2::New(160, 144),  UVec2::New(240, 160), UVec2::New(320, 240), UVec2::New(480, 272),
    UVec2::New(640, 360),  UVec2::New(800, 600), UVec2::New(960, 540), UVec2::New(1280, 720),
    UVec2::New(1600, 900), UVec2::New(1920, 1080),
};

const char* GetVideoSettingsMenuOptionName(VideoSettingsMenuOption option) {
    switch (option) {
    case VideoSettingsMenuOption::Resolution:
        return "Resolution";
    case VideoSettingsMenuOption::WindowSize:
        return "Window Size";
    case VideoSettingsMenuOption::Fullscreen:
        return "Fullscreen";
    case VideoSettingsMenuOption::Apply:
        return "Apply";
    case VideoSettingsMenuOption::Back:
        return "Back";
    }

    return "";
}

bool ApplyShouldBeAvailable(const State& state) {
    return state.video_settings_target_resolution_index.has_value() ||
           state.video_settings_target_fullscreen.has_value() ||
           state.video_settings_target_window_size_index.has_value();
}

bool WindowSizeAvailableToChange(const State& state, const Graphics& graphics) {
    if (state.video_settings_target_fullscreen) {
        return !*state.video_settings_target_fullscreen;
    }
    return !graphics.fullscreen;
}

void ProcessInputVideoSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)dt;
    const bool confirm_pressed = state.menu_inputs.confirm.pressed;
    const bool up_pressed = state.menu_inputs.up.pressed;
    const bool down_pressed = state.menu_inputs.down.pressed;
    const bool left_pressed = state.menu_inputs.left.pressed;
    const bool right_pressed = state.menu_inputs.right.pressed;
    const VideoUpOrDownOrNeither up_down = up_pressed ? VideoUpOrDownOrNeither::Up
                                                      : (down_pressed ? VideoUpOrDownOrNeither::Down
                                                                      : VideoUpOrDownOrNeither::Neither);
    const VideoLeftOrRightOrNeither left_right =
        left_pressed ? VideoLeftOrRightOrNeither::Left
                     : (right_pressed ? VideoLeftOrRightOrNeither::Right
                                      : VideoLeftOrRightOrNeither::Neither);

    switch (state.video_settings_menu_selection) {
    case VideoSettingsMenuOption::Resolution:
        if (up_down == VideoUpOrDownOrNeither::Up) {
            PlayMenuSoundCant(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Down) {
            state.video_settings_menu_selection =
                WindowSizeAvailableToChange(state, graphics) ? VideoSettingsMenuOption::WindowSize
                                                             : VideoSettingsMenuOption::Fullscreen;
            PlayMenuSoundCursorMove(audio);
        } else if (left_right != VideoLeftOrRightOrNeither::Neither) {
            if (!state.video_settings_target_resolution_index) {
                for (std::size_t i = 0; i < kResolutions.size(); ++i) {
                    if (kResolutions[i] == graphics.dims) {
                        state.video_settings_target_resolution_index = i;
                        break;
                    }
                }
            }
            if (state.video_settings_target_resolution_index) {
                std::size_t index = *state.video_settings_target_resolution_index;
                if (left_right == VideoLeftOrRightOrNeither::Left) {
                    if (index > 0) {
                        state.video_settings_target_resolution_index = index - 1;
                        PlayMenuSoundCursorMove(audio);
                    } else {
                        PlayMenuSoundCant(audio);
                    }
                } else {
                    if (index + 1 < kResolutions.size()) {
                        state.video_settings_target_resolution_index = index + 1;
                        PlayMenuSoundCursorMove(audio);
                    } else {
                        PlayMenuSoundCant(audio);
                    }
                }
                if (state.video_settings_target_resolution_index &&
                    kResolutions[*state.video_settings_target_resolution_index] == graphics.dims) {
                    state.video_settings_target_resolution_index.reset();
                }
            }
        }
        break;
    case VideoSettingsMenuOption::WindowSize:
        if (up_down == VideoUpOrDownOrNeither::Up) {
            state.video_settings_menu_selection = VideoSettingsMenuOption::Resolution;
            PlayMenuSoundCursorMove(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Down) {
            state.video_settings_menu_selection = VideoSettingsMenuOption::Fullscreen;
            PlayMenuSoundCursorMove(audio);
        } else if (left_right != VideoLeftOrRightOrNeither::Neither) {
            if (!state.video_settings_target_window_size_index) {
                for (std::size_t i = 0; i < kResolutions.size(); ++i) {
                    if (kResolutions[i] == graphics.window_dims) {
                        state.video_settings_target_window_size_index = i;
                        break;
                    }
                }
            }
            if (state.video_settings_target_window_size_index) {
                std::size_t index = *state.video_settings_target_window_size_index;
                if (left_right == VideoLeftOrRightOrNeither::Left) {
                    if (index > 0) {
                        state.video_settings_target_window_size_index = index - 1;
                        PlayMenuSoundCursorMove(audio);
                    } else {
                        PlayMenuSoundCant(audio);
                    }
                } else {
                    if (index + 1 < kResolutions.size()) {
                        state.video_settings_target_window_size_index = index + 1;
                        PlayMenuSoundCursorMove(audio);
                    } else {
                        PlayMenuSoundCant(audio);
                    }
                }
                if (state.video_settings_target_window_size_index &&
                    kResolutions[*state.video_settings_target_window_size_index] == graphics.window_dims) {
                    state.video_settings_target_window_size_index.reset();
                }
            }
        }
        break;
    case VideoSettingsMenuOption::Fullscreen:
        if (confirm_pressed) {
            PlayMenuSoundConfirm(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Up) {
            state.video_settings_menu_selection =
                WindowSizeAvailableToChange(state, graphics) ? VideoSettingsMenuOption::WindowSize
                                                             : VideoSettingsMenuOption::Resolution;
            PlayMenuSoundCursorMove(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Down) {
            state.video_settings_menu_selection =
                ApplyShouldBeAvailable(state) ? VideoSettingsMenuOption::Apply
                                              : VideoSettingsMenuOption::Back;
            PlayMenuSoundCursorMove(audio);
        } else if (left_right != VideoLeftOrRightOrNeither::Neither) {
            if (!state.video_settings_target_fullscreen) {
                state.video_settings_target_fullscreen = graphics.fullscreen;
            }
            state.video_settings_target_fullscreen = !*state.video_settings_target_fullscreen;
            if (*state.video_settings_target_fullscreen) {
                state.video_settings_target_window_size_index.reset();
            }
            if (*state.video_settings_target_fullscreen == graphics.fullscreen) {
                state.video_settings_target_fullscreen.reset();
            }
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case VideoSettingsMenuOption::Apply:
        if (confirm_pressed) {
            if (state.video_settings_target_resolution_index) {
                graphics.dims = kResolutions[*state.video_settings_target_resolution_index];
                state.settings.video.resolution = graphics.dims;
                state.rebuild_render_texture = true;
                state.video_settings_target_resolution_index.reset();
            }
            if (state.video_settings_target_fullscreen) {
                graphics.fullscreen = *state.video_settings_target_fullscreen;
                state.settings.video.fullscreen = graphics.fullscreen;
                SDL_SetWindowFullscreen(window, graphics.fullscreen);
                state.video_settings_target_fullscreen.reset();
            }
            if (!graphics.fullscreen && state.video_settings_target_window_size_index) {
                graphics.window_dims = kResolutions[*state.video_settings_target_window_size_index];
                SDL_SetWindowSize(window, static_cast<int>(graphics.window_dims.x), static_cast<int>(graphics.window_dims.y));
                state.video_settings_target_window_size_index.reset();
            }
            SaveSettings(state.settings);
            PlayMenuSoundConfirm(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Up) {
            state.video_settings_menu_selection = VideoSettingsMenuOption::Fullscreen;
            PlayMenuSoundCursorMove(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Down) {
            state.video_settings_menu_selection = VideoSettingsMenuOption::Back;
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case VideoSettingsMenuOption::Back:
        if (confirm_pressed) {
            state.SetMode(Mode::Settings);
            state.video_settings_target_resolution_index.reset();
            state.video_settings_target_fullscreen.reset();
            PlayMenuSoundCant(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Up) {
            state.video_settings_menu_selection =
                ApplyShouldBeAvailable(state) ? VideoSettingsMenuOption::Apply
                                              : VideoSettingsMenuOption::Fullscreen;
            PlayMenuSoundCursorMove(audio);
        } else if (up_down == VideoUpOrDownOrNeither::Down) {
            PlayMenuSoundCant(audio);
        }
        break;
    }
}

} // namespace splonks

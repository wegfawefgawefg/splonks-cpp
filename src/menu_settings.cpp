#include "menu_settings.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
#include "state.hpp"

namespace splonks {

const char* GetSettingsMenuOptionName(SettingsMenuOption option) {
    switch (option) {
    case SettingsMenuOption::Video:
        return "Video";
    case SettingsMenuOption::Audio:
        return "Audio";
    case SettingsMenuOption::Controls:
        return "Controls";
    case SettingsMenuOption::Back:
        return "Back";
    }

    return "";
}

void ProcessInputSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)window;
    (void)graphics;
    (void)dt;
    const bool confirm_pressed = state.menu_inputs.confirm.pressed;
    const bool up_pressed = state.menu_inputs.up.pressed;
    const bool down_pressed = state.menu_inputs.down.pressed;
    const SettingsUpOrDownOrNeither direction = up_pressed ? SettingsUpOrDownOrNeither::Up
                                                           : (down_pressed
                                                                  ? SettingsUpOrDownOrNeither::Down
                                                                  : SettingsUpOrDownOrNeither::Neither);

    switch (state.settings_menu_selection) {
    case SettingsMenuOption::Video:
        if (confirm_pressed) {
            state.SetMode(Mode::VideoSettings);
            state.video_settings_menu_selection = VideoSettingsMenuOption::Resolution;
            PlayMenuSoundConfirm(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Up) {
            PlayMenuSoundCant(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Down) {
            state.settings_menu_selection = SettingsMenuOption::Audio;
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case SettingsMenuOption::Audio:
        if (confirm_pressed) {
            state.SetMode(Mode::Settings);
            PlayMenuSoundConfirm(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Up) {
            state.settings_menu_selection = SettingsMenuOption::Video;
            PlayMenuSoundCursorMove(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Down) {
            state.settings_menu_selection = SettingsMenuOption::Controls;
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case SettingsMenuOption::Controls:
        if (confirm_pressed) {
            state.SetMode(Mode::Settings);
            PlayMenuSoundConfirm(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Up) {
            state.settings_menu_selection = SettingsMenuOption::Audio;
            PlayMenuSoundCursorMove(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Down) {
            state.settings_menu_selection = SettingsMenuOption::Back;
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case SettingsMenuOption::Back:
        if (confirm_pressed) {
            state.SetMode(Mode::Title);
            PlayMenuSoundCant(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Up) {
            state.settings_menu_selection = SettingsMenuOption::Controls;
            PlayMenuSoundCursorMove(audio);
        } else if (direction == SettingsUpOrDownOrNeither::Down) {
            PlayMenuSoundCant(audio);
        }
        break;
    }
}

} // namespace splonks

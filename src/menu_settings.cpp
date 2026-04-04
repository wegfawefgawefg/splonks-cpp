#include "menu_settings.hpp"

#include "audio.hpp"
#include "graphics.hpp"
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
    const SettingsUpOrDownOrNeither direction =
        state.menu_inputs.up ? SettingsUpOrDownOrNeither::Up
                             : (state.menu_inputs.down ? SettingsUpOrDownOrNeither::Down
                                                       : SettingsUpOrDownOrNeither::Neither);

    switch (state.settings_menu_selection) {
    case SettingsMenuOption::Video:
        if (state.menu_inputs.confirm) {
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
        if (state.menu_inputs.confirm) {
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
        if (state.menu_inputs.confirm) {
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
        if (state.menu_inputs.confirm) {
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

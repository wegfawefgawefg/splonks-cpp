#include "menu_title.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
#include "stage_init.hpp"
#include "state.hpp"

namespace splonks {

const char* GetTitleMenuOptionName(TitleMenuOption option) {
    switch (option) {
    case TitleMenuOption::Start:
        return "Start";
    case TitleMenuOption::Settings:
        return "Settings";
    case TitleMenuOption::Quit:
        return "Quit";
    }

    return "";
}

void ProcessInputTitle(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)graphics;
    (void)dt;
    (void)window;

    const bool confirm_pressed =
        GamepadButtonPressedEdge(SDL_GAMEPAD_BUTTON_START) ||
        GamepadButtonPressedEdge(SDL_GAMEPAD_BUTTON_SOUTH) ||
        KeyPressedEdge(SDL_SCANCODE_SPACE) || KeyPressedEdge(SDL_SCANCODE_RETURN) ||
        KeyPressedEdge(SDL_SCANCODE_KP_ENTER);
    const bool up_pressed = GamepadButtonPressedEdge(SDL_GAMEPAD_BUTTON_DPAD_UP) ||
                            KeyPressedEdge(SDL_SCANCODE_UP) || KeyPressedEdge(SDL_SCANCODE_W);
    const bool down_pressed = GamepadButtonPressedEdge(SDL_GAMEPAD_BUTTON_DPAD_DOWN) ||
                              KeyPressedEdge(SDL_SCANCODE_DOWN) || KeyPressedEdge(SDL_SCANCODE_S);

    //  TITLE MENU STATE MACHINE
    switch (state.title_menu_selection) {
    case TitleMenuOption::Start:
        if (confirm_pressed) {
            audio.PlaySong(Song::Playing);
            PlayMenuSoundSuperConfirm(audio);
            state.SetMode(Mode::StageTransition);
        } else {
            switch (up_pressed ? UpOrDownOrNeither::Up
                               : (down_pressed ? UpOrDownOrNeither::Down
                                                                   : UpOrDownOrNeither::Neither)) {
            case UpOrDownOrNeither::Up:
                PlayMenuSoundCant(audio);
                break;
            case UpOrDownOrNeither::Down:
                state.title_menu_selection = TitleMenuOption::Settings;
                PlayMenuSoundCursorMove(audio);
                break;
            case UpOrDownOrNeither::Neither:
                break;
            }
        }
        break;
    case TitleMenuOption::Settings:
        if (confirm_pressed) {
            state.SetMode(Mode::Settings);
            PlayMenuSoundConfirm(audio);
        } else {
            switch (up_pressed ? UpOrDownOrNeither::Up
                               : (down_pressed ? UpOrDownOrNeither::Down
                                                                   : UpOrDownOrNeither::Neither)) {
            case UpOrDownOrNeither::Up:
                state.title_menu_selection = TitleMenuOption::Start;
                PlayMenuSoundCursorMove(audio);
                break;
            case UpOrDownOrNeither::Down:
                state.title_menu_selection = TitleMenuOption::Quit;
                PlayMenuSoundCursorMove(audio);
                break;
            case UpOrDownOrNeither::Neither:
                break;
            }
        }
        break;
    case TitleMenuOption::Quit:
        if (confirm_pressed) {
            state.running = false;
        } else {
            switch (up_pressed ? UpOrDownOrNeither::Up
                               : (down_pressed ? UpOrDownOrNeither::Down
                                                                   : UpOrDownOrNeither::Neither)) {
            case UpOrDownOrNeither::Up:
                state.title_menu_selection = TitleMenuOption::Settings;
                PlayMenuSoundCursorMove(audio);
                break;
            case UpOrDownOrNeither::Down:
                PlayMenuSoundCant(audio);
                break;
            case UpOrDownOrNeither::Neither:
                break;
            }
        }
        break;
    }
}

} // namespace splonks

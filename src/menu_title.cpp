#include "menu_title.hpp"

#include "audio.hpp"
#include "graphics.hpp"
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
    //  TITLE MENU STATE MACHINE
    switch (state.title_menu_selection) {
    case TitleMenuOption::Start:
        if (state.menu_inputs.confirm) {
            audio.PlaySong(Song::Playing);
            PlayMenuSoundSuperConfirm(audio);
            state.SetMode(Mode::StageTransition);
        } else {
            switch (state.menu_inputs.up ? UpOrDownOrNeither::Up
                                         : (state.menu_inputs.down ? UpOrDownOrNeither::Down
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
        if (state.menu_inputs.confirm) {
            state.SetMode(Mode::Settings);
            PlayMenuSoundConfirm(audio);
        } else {
            switch (state.menu_inputs.up ? UpOrDownOrNeither::Up
                                         : (state.menu_inputs.down ? UpOrDownOrNeither::Down
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
        if (state.menu_inputs.confirm) {
            state.running = false;
        } else {
            switch (state.menu_inputs.up ? UpOrDownOrNeither::Up
                                         : (state.menu_inputs.down ? UpOrDownOrNeither::Down
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

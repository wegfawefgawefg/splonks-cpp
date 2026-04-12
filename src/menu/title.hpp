#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

enum class TitleMenuOption {
    Start,
    Settings,
    Quit,
};

enum class UpOrDownOrNeither {
    Neither,
    Up,
    Down,
};

const char* GetTitleMenuOptionName(TitleMenuOption option);
void ProcessInputTitle(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
);

} // namespace splonks

#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

enum class SettingsMenuOption {
    Video,
    Audio,
    Controls,
    Ui,
    Back,
};

enum class SettingsUpOrDownOrNeither {
    Neither,
    Up,
    Down,
};

enum class LeftOrRightOrNeither {
    Neither,
    Left,
    Right,
};

const char* GetSettingsMenuOptionName(SettingsMenuOption option);
void ProcessInputSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
);

} // namespace splonks

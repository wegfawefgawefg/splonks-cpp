#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

enum class UiSettingsMenuOption {
    IconScale,
    StatusIconScale,
    ToolSlotScale,
    ToolIconScale,
    Back,
};

const char* GetUiSettingsMenuOptionName(UiSettingsMenuOption option);
void ProcessInputUiSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
);

} // namespace splonks

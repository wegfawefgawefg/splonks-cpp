#pragma once

#include "math_types.hpp"

#include <SDL3/SDL.h>
#include <array>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

enum class VideoSettingsMenuOption {
    Resolution,
    WindowSize,
    Fullscreen,
    Apply,
    Back,
};

enum class VideoUpOrDownOrNeither {
    Neither,
    Up,
    Down,
};

enum class VideoLeftOrRightOrNeither {
    Neither,
    Left,
    Right,
};

extern const std::array<UVec2, 10> kResolutions;

const char* GetVideoSettingsMenuOptionName(VideoSettingsMenuOption option);
void ProcessInputVideoSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
);
bool ApplyShouldBeAvailable(const State& state);
bool WindowSizeAvailableToChange(const State& state, const Graphics& graphics);

} // namespace splonks

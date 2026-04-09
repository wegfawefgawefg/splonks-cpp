#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

enum class PostFxSettingsMenuOption {
    Effect,
    CrtScanlines,
    CrtScanlineEdgeStart,
    CrtScanlineEdgeFalloff,
    CrtScanlineEdgeStrength,
    CrtZoom,
    CrtWarp,
    CrtVignette,
    CrtVignetteIntensity,
    CrtGrille,
    CrtBrightness,
    Back,
};

const char* GetPostFxSettingsMenuOptionName(PostFxSettingsMenuOption option);
void ProcessInputPostFxSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
);

} // namespace splonks

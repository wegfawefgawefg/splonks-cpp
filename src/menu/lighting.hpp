#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

enum class LightingSettingsMenuOption {
    TerrainLighting,
    TerrainSeamAo,
    TerrainSeamAoAmount,
    TerrainSeamAoSize,
    TerrainExposureLighting,
    TerrainExposureAmount,
    TerrainExposureMinBrightness,
    TerrainExposureMaxBrightness,
    TerrainExposureDiagonalWeight,
    TerrainExposureSmoothing,
    BackwallLighting,
    BackwallBrightness,
    BackwallMinBrightness,
    BackwallMaxBrightness,
    BackwallSmoothing,
    Back,
};

const char* GetLightingSettingsMenuOptionName(LightingSettingsMenuOption option);
void ProcessInputLightingSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
);

} // namespace splonks

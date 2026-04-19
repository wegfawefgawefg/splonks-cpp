#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

void RenderDebugOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    State& state,
    const Audio& audio
);

} // namespace splonks

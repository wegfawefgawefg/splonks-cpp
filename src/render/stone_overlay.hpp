#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;
struct FVec2;

void RenderStoneEntOverlay(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const FVec2& render_position,
    const FVec2& render_size
);

} // namespace splonks

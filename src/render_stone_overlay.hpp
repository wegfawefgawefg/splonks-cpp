#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;
struct Vec2;

void RenderStoneEntityOverlay(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Vec2& render_position,
    const Vec2& render_size
);

} // namespace splonks

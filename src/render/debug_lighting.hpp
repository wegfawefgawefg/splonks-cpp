#pragma once

#include "math_types.hpp"

#include <SDL3/SDL.h>

#include <vector>

namespace splonks {

struct Graphics;
struct State;

void RenderLightOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& pres,
    const std::vector<Vec2>& render_offsets
);

} // namespace splonks

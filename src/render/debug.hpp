#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void RenderDebugOverlay(SDL_Renderer* renderer, Graphics& graphics, const State& state);

} // namespace splonks

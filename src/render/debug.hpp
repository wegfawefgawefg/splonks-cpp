#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void RenderDebugOverlay(SDL_Renderer* renderer, Graphics& graphics, State& state);

} // namespace splonks

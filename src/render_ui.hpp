#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void RenderHealthRopeBombs(SDL_Renderer* renderer, const State& state, Graphics& graphics);

} // namespace splonks

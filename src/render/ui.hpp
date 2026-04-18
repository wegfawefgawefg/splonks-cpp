#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void RenderPlayingHud(SDL_Renderer* renderer, const State& state, Graphics& graphics);
void RenderWorldPrompts(SDL_Renderer* renderer, const State& state, Graphics& graphics);

} // namespace splonks

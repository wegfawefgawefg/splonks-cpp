#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void Render(SDL_Renderer* renderer, SDL_Texture* render_texture, State& state, Graphics& graphics);

} // namespace splonks

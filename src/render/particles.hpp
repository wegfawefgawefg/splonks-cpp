#pragma once

#include "draw_layer.hpp"

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void RenderParticlesForLayer(SDL_Renderer* renderer, const State& state, Graphics& graphics, DrawLayer layer);

} // namespace splonks


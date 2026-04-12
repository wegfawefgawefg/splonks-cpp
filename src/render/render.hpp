#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct RenderPostFx;
struct State;

void Render(
    SDL_Renderer* renderer,
    SDL_Texture* render_texture,
    const RenderPostFx& post_fx,
    State& state,
    Graphics& graphics
);

} // namespace splonks

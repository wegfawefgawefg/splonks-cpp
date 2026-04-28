#pragma once

#include "math_types.hpp"

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;

Vec2 WorldToScreen(const Graphics& graphics, const Vec2& world_pos);
SDL_FRect WorldRectToScreen(const Graphics& graphics, const Vec2& world_pos, const Vec2& world_size);
void RenderWorldTextureRotated(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect* src,
    const SDL_FRect& dst,
    double local_rotation,
    const SDL_FPoint* local_center,
    SDL_FlipMode flip
);
void RenderWorldTexture(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect* src,
    const SDL_FRect& dst
);

} // namespace splonks

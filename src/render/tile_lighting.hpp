#pragma once

#include "math_types.hpp"
#include "tile.hpp"

struct SDL_Renderer;
struct SDL_FRect;
struct SDL_Texture;

namespace splonks {

struct Graphics;
struct State;

void RenderTerrainTileLighting(
    SDL_Renderer* renderer,
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y,
    const SDL_FRect& dst
);

bool RenderTerrainTileWithVertexLighting(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    const SDL_FRect& src,
    const SDL_FRect& dst,
    const Vec2& world_pos,
    TileRotation tile_rotation
);

void ApplyTerrainTileBrightness(
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
);

void ApplyBackwallTileBrightness(
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
);

void ResetTerrainTileBrightness(SDL_Texture* texture);

} // namespace splonks

#pragma once

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

#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void RenderStageTiles(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderStageTileWrapper(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderEmbeddedTreasureOverlays(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderBackgroundStamps(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderEntities(SDL_Renderer* renderer, const State& state, Graphics& graphics);
void RenderParticles(SDL_Renderer* renderer, const State& state, Graphics& graphics);

} // namespace splonks

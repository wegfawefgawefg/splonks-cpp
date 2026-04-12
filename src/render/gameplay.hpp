#pragma once

struct SDL_Renderer;

namespace splonks {

struct Graphics;
struct State;

void RenderPlaying(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderStageTransition(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderGameOver(SDL_Renderer* renderer, Graphics& graphics);
void RenderWin(SDL_Renderer* renderer, Graphics& graphics);

} // namespace splonks

#pragma once

struct SDL_Renderer;

namespace splonks {

struct Graphics;
struct State;

void RenderTitle(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderVideoSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderUiSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderPostFxSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics);
void RenderLightingSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics);

} // namespace splonks

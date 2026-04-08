#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Graphics;
struct State;

void RenderStageLayout(SDL_Renderer* renderer, Graphics& graphics, const State& state);
void RenderRoomsOverlay(SDL_Renderer* renderer, Graphics& graphics, const State& state);
void RenderEntityCollisionBoxes(SDL_Renderer* renderer, Graphics& graphics, const State& state);

} // namespace splonks

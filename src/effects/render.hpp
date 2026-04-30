#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Entity;
struct Graphics;
struct State;

void RenderEffectWorldOverlays(SDL_Renderer* renderer, const State& state, Graphics& graphics, const Entity& owner);
void RenderCompassWorldOverlay(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Entity& owner,
    const struct EffectInstance& effect
);

} // namespace splonks

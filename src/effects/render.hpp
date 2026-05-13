#pragma once

#include <SDL3/SDL.h>

namespace splonks {

struct Ent;
struct Graphics;
struct State;

void RenderEffectWorldOverlays(SDL_Renderer* renderer, const State& state, Graphics& graphics, const Ent& owner);
void RenderCompassWorldOverlay(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Ent& owner,
    const struct EffectInstance& effect
);

} // namespace splonks

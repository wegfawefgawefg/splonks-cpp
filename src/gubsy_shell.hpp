#pragma once

#include "graphics.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>

namespace splonks::gubsy_shell {

struct Shell {
    GubsyRuntime runtime;
};

bool Init(Shell& shell, State& state, SDL_Window* window, SDL_Renderer* renderer,
          const Graphics& graphics);
void BeginDebugFrame(Shell& shell, float dt);
void UpdateTitleMenu(Shell& shell, const State& state, float dt, int screen_width,
                     int screen_height);
void RenderTitleMenu(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height);
void RenderDebug(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height);
void ShutdownDebug(Shell& shell);

} // namespace splonks::gubsy_shell

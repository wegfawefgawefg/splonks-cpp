#pragma once

#include "math_types.hpp"

#include <SDL3/SDL.h>

namespace splonks {

struct MenuInputs {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool back = false;

    static MenuInputs New();
};

struct PlayingInputs {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool jump = false;
    bool run = false;
    bool use_button = false;
    bool equip_button = false;
    bool pick_up_drop = false;
    bool stop = false;
    bool bomb = false;
    bool rope = false;
    bool attack = false;
    UVec2 mouse_pos;

    static PlayingInputs New();
};

struct MenuInputDebounceTimers {
    float left = 0.0F;
    float right = 0.0F;
    float up = 0.0F;
    float down = 0.0F;

    static MenuInputDebounceTimers New();
    void Step(float dt);
    MenuInputs Debounce(const MenuInputs& menu_inputs) const;
    void ResetOnDiff(const MenuInputs& last_inputs, const MenuInputs& new_inputs);
};

struct Audio;
struct Graphics;
struct State;

void ProcessInput(
    SDL_Window* window,
    SDL_Renderer* renderer,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
);

} // namespace splonks

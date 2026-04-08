#pragma once

#include "math_types.hpp"

#include <SDL3/SDL.h>

namespace splonks {

bool KeyPressedEdge(SDL_Scancode scancode);
bool GamepadButtonPressedEdge(SDL_GamepadButton button);
bool GamepadButtonDown(SDL_GamepadButton button);
bool GamepadAxisPressed(SDL_GamepadAxis axis);

struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct MenuInputs {
    ButtonState left;
    ButtonState right;
    ButtonState up;
    ButtonState down;
    ButtonState confirm;
    ButtonState back;

    static MenuInputs New();
};

struct MenuInputSnapshot {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool back = false;

    static MenuInputSnapshot New();
};

struct PlayingInputs {
    ButtonState left;
    ButtonState right;
    ButtonState up;
    ButtonState down;
    ButtonState jump;
    ButtonState run;
    ButtonState use_button;
    ButtonState equip_button;
    ButtonState pick_up_drop;
    ButtonState stop;
    ButtonState bomb;
    ButtonState rope;
    ButtonState attack;
    ButtonState quit;
    ButtonState toggle_collision_boxes;
    ButtonState regenerate_level;
    UVec2 mouse_pos;

    static PlayingInputs New();
};

struct PlayingInputSnapshot {
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
    bool quit = false;
    bool toggle_collision_boxes = false;
    bool regenerate_level = false;
    UVec2 mouse_pos;

    static PlayingInputSnapshot New();
};

struct MenuInputDebounceTimers {
    float left = 0.0F;
    float right = 0.0F;
    float up = 0.0F;
    float down = 0.0F;

    static MenuInputDebounceTimers New();
    void Step(float dt);
    void ApplyRepeat(MenuInputs& menu_inputs);
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
void LatchPlayingInputsForTick(State& state);
void LatchMenuInputsForFrame(State& state, float dt);

} // namespace splonks

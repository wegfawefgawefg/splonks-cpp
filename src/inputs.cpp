#include "inputs.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "menu_settings.hpp"
#include "menu_title.hpp"
#include "menu_ui.hpp"
#include "menu_video.hpp"
#include "settings.hpp"
#include "stage.hpp"
#include "stage_init.hpp"
#include "state.hpp"

namespace splonks {

namespace {

SDL_Gamepad* GetFirstGamepad() {
    static SDL_Gamepad* cached_gamepad = nullptr;
    static SDL_JoystickID cached_id = 0;

    if (cached_gamepad != nullptr) {
        if (SDL_GamepadConnected(cached_gamepad)) {
            return cached_gamepad;
        }
        SDL_CloseGamepad(cached_gamepad);
        cached_gamepad = nullptr;
        cached_id = 0;
    }

    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr || count <= 0) {
        return nullptr;
    }

    const SDL_JoystickID first_id = gamepads[0];
    SDL_free(gamepads);
    cached_gamepad = SDL_OpenGamepad(first_id);
    if (cached_gamepad != nullptr) {
        cached_id = first_id;
    } else {
        cached_id = 0;
    }
    (void)cached_id;
    return cached_gamepad;
}

} // namespace

bool KeyPressedEdge(SDL_Scancode scancode) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    static bool previous[SDL_SCANCODE_COUNT]{};
    const bool pressed = keys[scancode] && !previous[scancode];
    previous[scancode] = keys[scancode];
    return pressed;
}

bool GamepadButtonPressedEdge(SDL_GamepadButton button) {
    static bool previous[SDL_GAMEPAD_BUTTON_COUNT]{};

    SDL_Gamepad* gamepad = GetFirstGamepad();
    if (gamepad == nullptr) {
        previous[button] = false;
        return false;
    }

    const bool current = SDL_GetGamepadButton(gamepad, button);
    const bool pressed = current && !previous[button];
    previous[button] = current;
    return pressed;
}

bool GamepadButtonDown(SDL_GamepadButton button) {
    SDL_Gamepad* gamepad = GetFirstGamepad();
    if (gamepad == nullptr) {
        return false;
    }

    return SDL_GetGamepadButton(gamepad, button);
}

bool GamepadAxisPressed(SDL_GamepadAxis axis) {
    SDL_Gamepad* gamepad = GetFirstGamepad();
    if (gamepad == nullptr) {
        return false;
    }

    constexpr Sint16 kTriggerThreshold = 16000;
    return SDL_GetGamepadAxis(gamepad, axis) > kTriggerThreshold;
}

namespace {

ButtonState BuildButtonState(bool down, bool previous_down) {
    return ButtonState{
        .down = down,
        .pressed = down && !previous_down,
        .released = !down && previous_down,
    };
}

PlayingInputs BuildPlayingInputs(
    const PlayingInputSnapshot& current,
    const PlayingInputSnapshot& previous
) {
    PlayingInputs inputs = PlayingInputs::New();
    inputs.left = BuildButtonState(current.left, previous.left);
    inputs.right = BuildButtonState(current.right, previous.right);
    inputs.up = BuildButtonState(current.up, previous.up);
    inputs.down = BuildButtonState(current.down, previous.down);
    inputs.jump = BuildButtonState(current.jump, previous.jump);
    inputs.run = BuildButtonState(current.run, previous.run);
    inputs.use_button = BuildButtonState(current.use_button, previous.use_button);
    inputs.equip_button = BuildButtonState(current.equip_button, previous.equip_button);
    inputs.pick_up_drop = BuildButtonState(current.pick_up_drop, previous.pick_up_drop);
    inputs.stop = BuildButtonState(current.stop, previous.stop);
    inputs.bomb = BuildButtonState(current.bomb, previous.bomb);
    inputs.rope = BuildButtonState(current.rope, previous.rope);
    inputs.attack = BuildButtonState(current.attack, previous.attack);
    inputs.quit = BuildButtonState(current.quit, previous.quit);
    inputs.toggle_collision_boxes = BuildButtonState(
        current.toggle_collision_boxes,
        previous.toggle_collision_boxes
    );
    inputs.regenerate_level = BuildButtonState(
        current.regenerate_level,
        previous.regenerate_level
    );
    inputs.mouse_pos = current.mouse_pos;
    return inputs;
}

void SetMenuInputSnapshot(State& state) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    MenuInputSnapshot new_inputs = MenuInputSnapshot::New();

    new_inputs.left =
        keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] ||
        GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    new_inputs.right =
        keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] ||
        GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    new_inputs.up =
        keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] ||
        GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_UP);
    new_inputs.down =
        keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] ||
        GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    new_inputs.confirm =
        keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE] ||
        GamepadButtonDown(SDL_GAMEPAD_BUTTON_SOUTH);
    new_inputs.back =
        keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE] ||
        GamepadButtonDown(SDL_GAMEPAD_BUTTON_EAST);
    state.menu_input_snapshot = new_inputs;
}

void SetPlayingInputSnapshot(State& state) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    PlayingInputSnapshot new_inputs = PlayingInputSnapshot::New();
    new_inputs.left = keys[SDL_SCANCODE_A] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    new_inputs.right = keys[SDL_SCANCODE_D] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    new_inputs.up = keys[SDL_SCANCODE_W] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_UP);
    new_inputs.down = keys[SDL_SCANCODE_S] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    new_inputs.jump = keys[SDL_SCANCODE_SPACE] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_SOUTH);
    new_inputs.run = keys[SDL_SCANCODE_LSHIFT] || GamepadAxisPressed(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    new_inputs.use_button = keys[SDL_SCANCODE_J] || GamepadAxisPressed(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    new_inputs.equip_button =
        keys[SDL_SCANCODE_I] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    new_inputs.pick_up_drop =
        keys[SDL_SCANCODE_K] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_WEST);
    new_inputs.stop = keys[SDL_SCANCODE_LCTRL] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_START);
    new_inputs.bomb = keys[SDL_SCANCODE_M] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_EAST);
    new_inputs.rope = keys[SDL_SCANCODE_O] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_NORTH);
    new_inputs.attack =
        keys[SDL_SCANCODE_H] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    new_inputs.quit = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_Q];
    new_inputs.toggle_collision_boxes = false;
    new_inputs.regenerate_level =
        keys[SDL_SCANCODE_R] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_BACK);
    float mx = 0.0F;
    float my = 0.0F;
    SDL_GetMouseState(&mx, &my);
    new_inputs.mouse_pos = UVec2::New(static_cast<unsigned int>(mx), static_cast<unsigned int>(my));
    state.playing_input_snapshot = new_inputs;
}

void ProcessInputPlaying(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)audio;
    (void)dt;
    const PlayingInputs& inputs = state.immediate_playing_inputs;
    if (inputs.quit.pressed) {
        state.running = false;
    }

    if (inputs.regenerate_level.pressed) {
        InitDebugLevel(state);
        graphics.ResetTileVariations();
    }
    (void)window;
    (void)graphics;
}

void ProcessInputStageTransition(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)window;
    (void)audio;
    (void)dt;
    if (state.menu_inputs.back.pressed) {
        state.running = false;
    }

    if (state.menu_inputs.confirm.down && state.scene_frame >= 60) {
        if (state.next_stage) {
            state.stage = Stage::New(*state.next_stage);
            InitStage(state);
            graphics.ResetTileVariations();
            state.scene_frame = 0;
            state.SetMode(Mode::Playing);
        } else {
            state.SetMode(Mode::Win);
        }
    }
    (void)graphics;
}

void ProcessInputGameOver(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)window;
    (void)audio;
    (void)dt;
    if (state.menu_inputs.confirm.down) {
        if (state.next_stage != std::optional<StageType>(StageType::Test1)) {
            state.next_stage = StageType::Cave1;
        }
        graphics.camera.rotation = 0.0F;
        state.SetMode(Mode::StageTransition);
    }
}

void ProcessInputWin(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)window;
    (void)audio;
    (void)dt;
    if (state.menu_inputs.confirm.down &&
        state.scene_frame >= (60 * 5)) {
        state.stage = Stage::NewBlank();
        graphics.ResetTileVariations();
        state.SetMode(Mode::Title);
    }
    (void)graphics;
}

} // namespace

MenuInputs MenuInputs::New() {
    return MenuInputs{};
}

MenuInputSnapshot MenuInputSnapshot::New() {
    return MenuInputSnapshot{};
}

PlayingInputs PlayingInputs::New() {
    PlayingInputs inputs;
    inputs.mouse_pos = UVec2::New(0, 0);
    return inputs;
}

PlayingInputSnapshot PlayingInputSnapshot::New() {
    PlayingInputSnapshot inputs;
    inputs.mouse_pos = UVec2::New(0, 0);
    return inputs;
}

MenuInputDebounceTimers MenuInputDebounceTimers::New() {
    return MenuInputDebounceTimers{};
}

void MenuInputDebounceTimers::Step(float dt) {
    left = Max(left - dt, 0.0F);
    right = Max(right - dt, 0.0F);
    up = Max(up - dt, 0.0F);
    down = Max(down - dt, 0.0F);
}

void MenuInputDebounceTimers::ApplyRepeat(MenuInputs& menu_inputs) {
    if (menu_inputs.left.down && left <= 0.0F) {
        menu_inputs.left.pressed = true;
        left = kKeyDebounceInterval;
    }
    if (menu_inputs.right.down && right <= 0.0F) {
        menu_inputs.right.pressed = true;
        right = kKeyDebounceInterval;
    }
    if (menu_inputs.up.down && up <= 0.0F) {
        menu_inputs.up.pressed = true;
        up = kKeyDebounceInterval;
    }
    if (menu_inputs.down.down && down <= 0.0F) {
        menu_inputs.down.pressed = true;
        down = kKeyDebounceInterval;
    }
}

void ProcessInput(
    SDL_Window* window,
    SDL_Renderer* renderer,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)renderer;
    SDL_PumpEvents();

    switch (state.mode) {
    case Mode::Playing:
        SetPlayingInputSnapshot(state);
        state.immediate_playing_inputs = BuildPlayingInputs(
            state.playing_input_snapshot,
            state.previous_immediate_playing_input_snapshot
        );
        state.previous_immediate_playing_input_snapshot = state.playing_input_snapshot;
        break;
    default:
        SetMenuInputSnapshot(state);
        LatchMenuInputsForFrame(state, dt);
        break;
    }

    switch (state.mode) {
    case Mode::Title:
        ProcessInputTitle(window, state, audio, graphics, dt);
        break;
    case Mode::Settings:
        ProcessInputSettingsMenu(window, state, audio, graphics, dt);
        break;
    case Mode::VideoSettings:
        ProcessInputVideoSettingsMenu(window, state, audio, graphics, dt);
        break;
    case Mode::UiSettings:
        ProcessInputUiSettingsMenu(window, state, audio, graphics, dt);
        break;
    case Mode::Playing:
        ProcessInputPlaying(window, state, audio, graphics, dt);
        break;
    case Mode::StageTransition:
        ProcessInputStageTransition(window, state, audio, graphics, dt);
        break;
    case Mode::GameOver:
        ProcessInputGameOver(window, state, audio, graphics, dt);
        break;
    case Mode::Win:
        ProcessInputWin(window, state, audio, graphics, dt);
        break;
    }
}

void LatchMenuInputsForFrame(State& state, float dt) {
    const MenuInputSnapshot current = state.menu_input_snapshot;
    const MenuInputSnapshot previous = state.previous_menu_input_snapshot;

    MenuInputs inputs = MenuInputs::New();
    inputs.left = BuildButtonState(current.left, previous.left);
    inputs.right = BuildButtonState(current.right, previous.right);
    inputs.up = BuildButtonState(current.up, previous.up);
    inputs.down = BuildButtonState(current.down, previous.down);
    inputs.confirm = BuildButtonState(current.confirm, previous.confirm);
    inputs.back = BuildButtonState(current.back, previous.back);

    state.menu_input_debounce_timers.Step(dt);
    state.menu_input_debounce_timers.ApplyRepeat(inputs);

    state.menu_inputs = inputs;
    state.previous_menu_input_snapshot = current;
}

void LatchPlayingInputsForTick(State& state) {
    const PlayingInputSnapshot current = state.playing_input_snapshot;
    state.playing_inputs = BuildPlayingInputs(current, state.previous_playing_input_snapshot);
    state.previous_playing_input_snapshot = current;
}

} // namespace splonks

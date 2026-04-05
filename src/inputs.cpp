#include "inputs.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "menu_settings.hpp"
#include "menu_title.hpp"
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

void SetMenuInputs(State& state, float dt) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const MenuInputs last_inputs = state.menu_inputs;
    MenuInputs new_inputs = MenuInputs::New();

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

    state.menu_input_debounce_timers.Step(dt);
    new_inputs = state.menu_input_debounce_timers.Debounce(new_inputs);
    state.menu_input_debounce_timers.ResetOnDiff(last_inputs, new_inputs);
    state.menu_inputs = new_inputs;
}

void SetPlayingInputs(State& state) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    PlayingInputs new_inputs = PlayingInputs::New();
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
    float mx = 0.0F;
    float my = 0.0F;
    SDL_GetMouseState(&mx, &my);
    new_inputs.mouse_pos = UVec2::New(static_cast<unsigned int>(mx), static_cast<unsigned int>(my));
    state.playing_inputs = new_inputs;
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
    if (KeyPressedEdge(SDL_SCANCODE_ESCAPE) || KeyPressedEdge(SDL_SCANCODE_Q)) {
        state.running = false;
    }

    // randomize the stage tiles
    if (KeyPressedEdge(SDL_SCANCODE_R) || GamepadButtonDown(SDL_GAMEPAD_BUTTON_BACK)) {
        if (state.next_stage) {
            state.stage = Stage::New(*state.next_stage);
            InitStage(state);
            graphics.ResetTileVariations();
        }
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
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (KeyPressedEdge(SDL_SCANCODE_ESCAPE) || KeyPressedEdge(SDL_SCANCODE_Q)) {
        state.running = false;
    }

    const bool proceed =
        keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_RETURN] ||
        GamepadButtonDown(SDL_GAMEPAD_BUTTON_SOUTH);
    if (proceed && state.scene_frame >= 60) {
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
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_SPACE] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_SOUTH)) {
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
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if ((keys[SDL_SCANCODE_SPACE] || GamepadButtonDown(SDL_GAMEPAD_BUTTON_SOUTH)) &&
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

PlayingInputs PlayingInputs::New() {
    PlayingInputs inputs;
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

MenuInputs MenuInputDebounceTimers::Debounce(const MenuInputs& menu_inputs) const {
    MenuInputs result;
    result.left = left <= 0.0F && menu_inputs.left;
    result.right = right <= 0.0F && menu_inputs.right;
    result.up = up <= 0.0F && menu_inputs.up;
    result.down = down <= 0.0F && menu_inputs.down;
    result.confirm = menu_inputs.confirm;
    result.back = menu_inputs.back;
    return result;
}

void MenuInputDebounceTimers::ResetOnDiff(const MenuInputs& last_inputs,
                                          const MenuInputs& new_inputs) {
    if (new_inputs.left && !last_inputs.left) {
        left = kKeyDebounceInterval;
    }
    if (new_inputs.right && !last_inputs.right) {
        right = kKeyDebounceInterval;
    }
    if (new_inputs.up && !last_inputs.up) {
        up = kKeyDebounceInterval;
    }
    if (new_inputs.down && !last_inputs.down) {
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
        SetPlayingInputs(state);
        break;
    default:
        SetMenuInputs(state, dt);
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

} // namespace splonks

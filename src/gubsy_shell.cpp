#include "gubsy_shell.hpp"

#include "input_bind_schema.hpp"
#include "inputs.hpp"

#include <filesystem>

namespace splonks::gubsy_shell {

namespace {

void StartSplonksFromGubsy(void* user_data, std::int32_t) {
    auto* state = static_cast<State*>(user_data);
    if (state == nullptr)
        return;
    state->SetMode(Mode::StageTransition);
}

void QuitSplonksFromGubsy(void* user_data, std::int32_t) {
    auto* state = static_cast<State*>(user_data);
    if (state == nullptr)
        return;
    state->running = false;
}

MenuInputState BuildGubsyMenuInput(const MenuInputs& inputs) {
    MenuInputState result{};
    result.up = inputs.up.down;
    result.down = inputs.down.down;
    result.left = inputs.left.down;
    result.right = inputs.right.down;
    result.select = inputs.confirm.down;
    result.back = inputs.back.down;
    return result;
}

GubsyAppConfig BuildGubsyConfig() {
    GubsyAppConfig config{};
    config.enable_mods = false;
    config.project_root = std::filesystem::current_path().string();
    config.data_root = (std::filesystem::current_path() / "data" / "gubsy").string();
    config.engine_assets_root = (std::filesystem::current_path() / "assets").string();
    return config;
}

} // namespace

bool Init(Shell& shell, State& state, SDL_Window* window, SDL_Renderer* renderer,
          const Graphics& graphics) {
    if (!init_gubsy_runtime(shell.runtime, BuildGubsyConfig()))
        return false;

    gubsy_register_binds_schema(shell.runtime, BuildGubsyBindsSchema());

    if (!gubsy_attach_sdl_renderer(shell.runtime, window, renderer,
                                   static_cast<int>(graphics.dims.x),
                                   static_cast<int>(graphics.dims.y))) {
        return false;
    }

    GubsyMainMenuCommands commands{};
    commands.start_game = gubsy_register_menu_command(shell.runtime, StartSplonksFromGubsy, &state);
    commands.quit = gubsy_register_menu_command(shell.runtime, QuitSplonksFromGubsy, &state);
    gubsy_set_main_menu_commands(shell.runtime, commands);
    return gubsy_show_main_menu(shell.runtime);
}

void BeginDebugFrame(Shell& shell, float dt) {
    gubsy_begin_debug_frame(shell.runtime, dt);
}

void UpdateTitleMenu(Shell& shell, const State& state, float dt, int screen_width,
                     int screen_height) {
    gubsy_set_menu_input(shell.runtime, BuildGubsyMenuInput(state.menu_inputs));
    gubsy_update_menu(shell.runtime, dt, screen_width, screen_height);
}

void RenderTitleMenu(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height) {
    gubsy_render_menu(shell.runtime, renderer, screen_width, screen_height);
}

void RenderDebug(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height) {
    gubsy_render_debug(shell.runtime, renderer, screen_width, screen_height);
}

void ShutdownDebug(Shell& shell) {
    gubsy_shutdown_debug(shell.runtime);
}

} // namespace splonks::gubsy_shell

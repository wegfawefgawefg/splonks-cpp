#include "gubsy_shell.hpp"

#include "input_bind_schema.hpp"
#include "inputs.hpp"
#include "network/net_lobby.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

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

std::string AdvertisedHost() {
    if (const char* value = std::getenv("SPLONKS_ADVERTISE_HOST")) {
        if (*value != '\0')
            return value;
    }
    return "127.0.0.1";
}

GubsyLobbyHostResult HostSplonksFromGubsy(void* user_data,
                                          const GubsyLobbyState&,
                                          std::uint16_t port) {
    GubsyLobbyHostResult result;
    auto* state = static_cast<State*>(user_data);
    if (state == nullptr)
        return result;
    result.ok = network::StartHostSession(*state, port, &result.status);
    if (result.ok) {
        std::uint16_t bound_port = network::BoundTransportPort(*state);
        if (bound_port == 0)
            bound_port = port;
        result.advertised_endpoint = AdvertisedHost() + ":" + std::to_string(bound_port);
    }
    std::cerr << result.status << '\n';
    return result;
}

GubsyLobbyJoinResult JoinSplonksFromGubsy(void* user_data,
                                          const GubsyLobbyState&,
                                          const char* host,
                                          std::uint16_t port) {
    GubsyLobbyJoinResult result;
    auto* state = static_cast<State*>(user_data);
    if (state == nullptr || host == nullptr || *host == '\0')
        return result;
    result.ok = network::JoinHostSession(*state, host, port, &result.status);
    std::cerr << result.status << '\n';
    return result;
}

GubsyLobbyLeaveResult LeaveSplonksFromGubsy(void* user_data, const GubsyLobbyState&) {
    GubsyLobbyLeaveResult result;
    auto* state = static_cast<State*>(user_data);
    if (state == nullptr)
        return result;
    network::DisconnectSession(*state, &result.status);
    result.ok = true;
    if (!result.status.empty())
        std::cerr << result.status << '\n';
    return result;
}

MenuInputState BuildGubsyMenuInput(const MenuInputs& inputs, bool text_edit_active) {
    MenuInputState result{};
    result.up = inputs.up.down;
    result.down = inputs.down.down;
    result.left = inputs.left.down;
    result.right = inputs.right.down;
    result.select = inputs.confirm.down;
    result.back = !text_edit_active && inputs.back.down;
    return result;
}

GubsyAppConfig BuildGubsyConfig(const Settings* settings = nullptr) {
    GubsyAppConfig config{};
    config.enable_mods = false;
    config.project_root = std::filesystem::current_path().string();
    config.data_root = (std::filesystem::current_path() / "data" / "gubsy").string();
    config.engine_assets_root = (std::filesystem::current_path() / "assets").string();
    config.window_title = "Splonks";
    config.utility_window = true;
    config.always_on_top = false;
    config.resizable_window = true;
    if (settings != nullptr) {
        config.window_width = static_cast<int>(settings->video.resolution.x);
        config.window_height = static_cast<int>(settings->video.resolution.y);
        config.render_width = static_cast<int>(settings->video.resolution.x);
        config.render_height = static_cast<int>(settings->video.resolution.y);
    }
    return config;
}

bool RegisterShellMenu(Shell& shell, State& state) {
    gubsy_register_binds_schema(shell.runtime, BuildGubsyBindsSchema());

    GubsyMainMenuCommands commands{};
    commands.start_game = gubsy_register_menu_command(shell.runtime, StartSplonksFromGubsy, &state);
    commands.quit = gubsy_register_menu_command(shell.runtime, QuitSplonksFromGubsy, &state);
    gubsy_set_main_menu_commands(shell.runtime, commands);

    GubsyLobbyCommands lobby_commands{};
    lobby_commands.host = HostSplonksFromGubsy;
    lobby_commands.host_user_data = &state;
    lobby_commands.join = JoinSplonksFromGubsy;
    lobby_commands.join_user_data = &state;
    lobby_commands.leave = LeaveSplonksFromGubsy;
    lobby_commands.leave_user_data = &state;
    gubsy_set_lobby_commands(shell.runtime, lobby_commands);
    return gubsy_show_main_menu(shell.runtime);
}

} // namespace

bool Init(Shell& shell, State& state, SDL_Window* window, SDL_Renderer* renderer,
          const Graphics& graphics) {
    if (!init_gubsy_runtime(shell.runtime, BuildGubsyConfig()))
        return false;

    if (!gubsy_attach_sdl_renderer(shell.runtime, window, renderer,
                                   static_cast<int>(graphics.dims.x),
                                   static_cast<int>(graphics.dims.y))) {
        return false;
    }

    return RegisterShellMenu(shell, state);
}

bool InitOwned(Shell& shell, State& state, const Settings& settings) {
    if (!init_gubsy_runtime(shell.runtime, BuildGubsyConfig(&settings)))
        return false;
    if (!gubsy_init_sdl_renderer(shell.runtime))
        return false;
    return RegisterShellMenu(shell, state);
}

GubsyFrame GetFrame(Shell& shell) {
    return gubsy_get_frame(shell.runtime);
}

void ProcessEvent(Shell& shell, const SDL_Event& event) {
    gubsy_process_sdl_event(shell.runtime, event);
}

void UpdateDeviceState(Shell& shell) {
    gubsy_update_device_state(shell.runtime);
}

bool TextEditActive(Shell& shell) {
    return gubsy_menu_text_edit_active(shell.runtime);
}

bool DrawFrameToWindow(Shell& shell) {
    return gubsy_draw_frame_to_window(shell.runtime);
}

void PresentFrame(Shell& shell) {
    gubsy_present_frame(shell.runtime);
}

int ConfiguredFrameCapFps(Shell& shell) {
    return gubsy_configured_frame_cap_fps(shell.runtime);
}

void BeginDebugFrame(Shell& shell, float dt) {
    gubsy_begin_debug_frame(shell.runtime, dt);
}

void UpdateTitleMenu(Shell& shell, const State& state, float dt, int screen_width,
                     int screen_height) {
    gubsy_set_menu_input(shell.runtime,
                         BuildGubsyMenuInput(state.menu_inputs, TextEditActive(shell)));
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

void Shutdown(Shell& shell) {
    gubsy_shutdown_debug(shell.runtime);
    cleanup_gubsy_runtime(shell.runtime);
}

} // namespace splonks::gubsy_shell

#include "gubsy_shell.hpp"

#include "input_bind_schema.hpp"
#include "inputs.hpp"
#include "network/net_lobby.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace splonks::gubsy_shell {

namespace {

struct CharacterOption {
    const char* id;
    const char* label;
    EntType type;
};

constexpr CharacterOption kCharacterOptions[] = {
    {"player", "Player", EntType::Player},
    {"bee", "Bee", EntType::FlappyBee},
    {"flesh_guy", "Flesh Guy", EntType::FleshGuy},
};

struct RespawnOption {
    const char* id;
    const char* label;
    const char* description;
    MultiplayerRespawnMode mode;
};

constexpr RespawnOption kRespawnOptions[] = {
    {"next_level", "Next Level", "Dead local players return at the next level transition.",
     MultiplayerRespawnMode::GenerousNextLevel},
    {"no_respawn", "No Respawn", "Dead local players stay dead.",
     MultiplayerRespawnMode::NoRespawn},
    {"entrance", "Entrance", "Dead network players can return at the level entrance.",
     MultiplayerRespawnMode::RespawnAtEntrance},
};

int FindRespawnOption(MultiplayerRespawnMode mode) {
    for (int i = 0; i < static_cast<int>(std::size(kRespawnOptions)); ++i) {
        if (kRespawnOptions[i].mode == mode)
            return i;
    }
    return 0;
}

const RespawnOption* FindRespawnOptionById(const std::string& id) {
    for (const RespawnOption& option : kRespawnOptions) {
        if (id == option.id)
            return &option;
    }
    return nullptr;
}

int FindCharacterOption(EntType type) {
    for (int i = 0; i < static_cast<int>(std::size(kCharacterOptions)); ++i) {
        if (kCharacterOptions[i].type == type)
            return i;
    }
    return 0;
}

const CharacterOption* FindCharacterOptionById(const std::string& id) {
    for (const CharacterOption& option : kCharacterOptions) {
        if (id == option.id)
            return &option;
    }
    return nullptr;
}

void EnsureLobbyConfigDefaults(Shell& shell, const GubsyLobbyState& lobby) {
    const std::size_t player_count = std::max<std::size_t>(lobby.local_players.size(), 1);
    shell.lobby_config.character_by_player.resize(player_count, EntType::Player);
    for (EntType& type : shell.lobby_config.character_by_player) {
        if (!IsPlayerLikeEntType(type))
            type = EntType::Player;
    }
}

void ApplyLobbyConfigToSplonks(Shell& shell, const GubsyLobbyState& lobby,
                               bool rebuild_local_players) {
    if (shell.state == nullptr)
        return;

    EnsureLobbyConfigDefaults(shell, lobby);
    shell.state->multiplayer_respawn_mode = shell.lobby_config.respawn_mode;

    if (!rebuild_local_players)
        return;

    const std::size_t player_count = std::max<std::size_t>(lobby.local_players.size(), 1);
    for (PlayerId player_id = 1; player_id <= static_cast<PlayerId>(player_count); ++player_id) {
        const std::size_t index = static_cast<std::size_t>(player_id - 1);
        PlayerSlot& slot = shell.state->players.EnsureLocalPlayer(
            player_id, "Player " + std::to_string(player_id), player_id == kPrimaryLocalPlayerId);
        slot.preferred_spawn_type = shell.lobby_config.character_by_player[index];
    }

    shell.state->players.slots.erase(
        std::remove_if(shell.state->players.slots.begin(), shell.state->players.slots.end(),
                       [player_count](const PlayerSlot& slot) {
                           return slot.connection_kind == PlayerConnectionKind::Local &&
                                  (slot.player_id < 1 ||
                                   slot.player_id > static_cast<PlayerId>(player_count));
                       }),
        shell.state->players.slots.end());
}

void EnsureSplonksLobbyDefaults(void* user_data, GubsyLobbyState& lobby) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr)
        return;
    EnsureLobbyConfigDefaults(*shell, lobby);
}

void BuildSplonksLobbyRows(void* user_data, const GubsyLobbyState& lobby,
                           std::vector<GubsyLobbyConfigRow>& out) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr)
        return;
    EnsureLobbyConfigDefaults(*shell, lobby);

    GubsyLobbyConfigRow respawn;
    respawn.key = "respawn_policy";
    respawn.label = "Respawn Policy";
    respawn.description = "Controls how dead players return during campaign co-op.";
    respawn.selected_option = FindRespawnOption(shell->lobby_config.respawn_mode);
    respawn.host_only = true;
    for (const RespawnOption& option : kRespawnOptions) {
        respawn.options.push_back({option.id, option.label, option.description});
    }
    out.push_back(std::move(respawn));

    for (int i = 0; i < static_cast<int>(lobby.local_players.size()); ++i) {
        const EntType type = shell->lobby_config.character_by_player[static_cast<std::size_t>(i)];
        GubsyLobbyConfigRow character;
        character.key = "character";
        character.label = "Player " + std::to_string(i + 1) + " Character";
        character.description = "Select the player body spawned for this local player.";
        character.player_index = i;
        character.selected_option = FindCharacterOption(type);
        character.host_only = false;
        for (const CharacterOption& option : kCharacterOptions) {
            character.options.push_back({option.id, option.label, ""});
        }
        out.push_back(std::move(character));
    }
}

bool SetSplonksLobbyOption(void* user_data, GubsyLobbyState& lobby, const char* key,
                           int player_index, int option_index) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || key == nullptr)
        return false;
    EnsureLobbyConfigDefaults(*shell, lobby);

    const std::string key_text = key;
    if (key_text == "respawn_policy") {
        if (option_index < 0 || option_index >= static_cast<int>(std::size(kRespawnOptions)))
            return false;
        shell->lobby_config.respawn_mode = kRespawnOptions[option_index].mode;
        ApplyLobbyConfigToSplonks(*shell, lobby, false);
        return true;
    }

    if (key_text == "character") {
        if (player_index < 0 ||
            player_index >= static_cast<int>(shell->lobby_config.character_by_player.size()) ||
            option_index < 0 || option_index >= static_cast<int>(std::size(kCharacterOptions))) {
            return false;
        }
        shell->lobby_config.character_by_player[static_cast<std::size_t>(player_index)] =
            kCharacterOptions[option_index].type;
        ApplyLobbyConfigToSplonks(*shell, lobby, !lobby.online);
        return true;
    }

    return false;
}

nlohmann::json SerializeSplonksLobbyConfig(void* user_data, const GubsyLobbyState& lobby) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr)
        return nlohmann::json::object();
    EnsureLobbyConfigDefaults(*shell, lobby);

    nlohmann::json players = nlohmann::json::array();
    for (int i = 0; i < static_cast<int>(lobby.local_players.size()); ++i) {
        const EntType type = shell->lobby_config.character_by_player[static_cast<std::size_t>(i)];
        const CharacterOption& option = kCharacterOptions[FindCharacterOption(type)];
        players.push_back({{"local_index", i}, {"character", option.id}});
    }

    const RespawnOption& respawn =
        kRespawnOptions[FindRespawnOption(shell->lobby_config.respawn_mode)];
    return nlohmann::json{
        {"mode", "campaign"},
        {"respawn_policy", respawn.id},
        {"players", players},
    };
}

bool ValidateSplonksLobbyConfig(void* user_data, const GubsyLobbyState& lobby,
                                std::string& message) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr) {
        message = "Splonks lobby config is not registered";
        return false;
    }
    EnsureLobbyConfigDefaults(*shell, lobby);
    return true;
}

bool ValidateSplonksRemoteConfig(void*, const GubsyLobbyState&, const SessionContract& remote,
                                 std::string& message) {
    const nlohmann::json& config = remote.game_config;
    if (!config.is_object()) {
        message = "Remote Splonks config is not an object";
        return false;
    }
    if (config.value("mode", "") != "campaign") {
        message = "Remote Splonks mode is not campaign";
        return false;
    }
    if (FindRespawnOptionById(config.value("respawn_policy", "")) == nullptr) {
        message = "Remote Splonks respawn policy is unknown";
        return false;
    }

    const auto players_it = config.find("players");
    if (players_it == config.end())
        return true;
    if (!players_it->is_array()) {
        message = "Remote Splonks players config is not a list";
        return false;
    }
    for (const nlohmann::json& player : *players_it) {
        if (!player.is_object()) {
            message = "Remote Splonks player config is invalid";
            return false;
        }
        if (FindCharacterOptionById(player.value("character", "")) == nullptr) {
            message = "Remote Splonks character config is unknown";
            return false;
        }
    }
    return true;
}

bool ApplySplonksRemoteConfig(void* user_data, GubsyLobbyState& lobby,
                              const SessionContract& remote, std::string& message) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr) {
        message = "Splonks lobby config is not registered";
        return false;
    }
    if (!ValidateSplonksRemoteConfig(user_data, lobby, remote, message))
        return false;

    const RespawnOption* respawn =
        FindRespawnOptionById(remote.game_config.value("respawn_policy", ""));
    if (respawn != nullptr)
        shell->lobby_config.respawn_mode = respawn->mode;
    ApplyLobbyConfigToSplonks(*shell, lobby, false);
    return true;
}

void StartSplonksFromGubsy(void* user_data, std::int32_t) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr)
        return;
    ApplyLobbyConfigToSplonks(*shell, gubsy_get_lobby_state(shell->runtime),
                              shell->state->net_session.role == network::NetRole::Offline);
    shell->state->SetMode(Mode::StageTransition);
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

GubsyLobbyHostResult HostSplonksFromGubsy(void* user_data, const GubsyLobbyState& lobby,
                                          std::uint16_t port) {
    GubsyLobbyHostResult result;
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr)
        return result;
    ApplyLobbyConfigToSplonks(*shell, lobby, true);
    result.ok = network::StartHostSession(*shell->state, port, &result.status);
    if (result.ok) {
        std::uint16_t bound_port = network::BoundTransportPort(*shell->state);
        if (bound_port == 0)
            bound_port = port;
        result.advertised_endpoint = AdvertisedHost() + ":" + std::to_string(bound_port);
    }
    std::cerr << result.status << '\n';
    return result;
}

GubsyLobbyJoinResult JoinSplonksFromGubsy(void* user_data, const GubsyLobbyState& lobby,
                                          const char* host, std::uint16_t port) {
    GubsyLobbyJoinResult result;
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr || host == nullptr || *host == '\0')
        return result;
    ApplyLobbyConfigToSplonks(*shell, lobby, true);
    result.ok = network::JoinHostSession(*shell->state, host, port, &result.status);
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
    shell.state = &state;
    gubsy_register_binds_schema(shell.runtime, BuildGubsyBindsSchema());

    GubsyMainMenuCommands commands{};
    commands.start_game = gubsy_register_menu_command(shell.runtime, StartSplonksFromGubsy, &shell);
    commands.quit = gubsy_register_menu_command(shell.runtime, QuitSplonksFromGubsy, &state);
    gubsy_set_main_menu_commands(shell.runtime, commands);

    GubsyLobbyConfigProvider config_provider{};
    config_provider.user_data = &shell;
    config_provider.ensure_defaults = EnsureSplonksLobbyDefaults;
    config_provider.build_rows = BuildSplonksLobbyRows;
    config_provider.set_option = SetSplonksLobbyOption;
    config_provider.serialize = SerializeSplonksLobbyConfig;
    config_provider.validate = ValidateSplonksLobbyConfig;
    config_provider.validate_remote = ValidateSplonksRemoteConfig;
    config_provider.apply_remote = ApplySplonksRemoteConfig;
    gubsy_set_lobby_config_provider(shell.runtime, config_provider);

    GubsyLobbyCommands lobby_commands{};
    lobby_commands.host = HostSplonksFromGubsy;
    lobby_commands.host_user_data = &shell;
    lobby_commands.join = JoinSplonksFromGubsy;
    lobby_commands.join_user_data = &shell;
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

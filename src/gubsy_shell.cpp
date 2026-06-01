#include "gubsy_shell.hpp"

#include "gubsy_shell_binds.hpp"
#include "input_bind_schema.hpp"
#include "inputs.hpp"
#include "network/net_lobby.hpp"
#include "stage_progression.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>

namespace splonks::gubsy_shell {

namespace {

constexpr std::uint64_t kDirectJoinTimeoutMs = 3500;

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

void SuppressGameplayInputAfterMenu(State& state);

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

    if (shell->state->net_session.role == network::NetRole::Peer) {
        if (shell->state->pending_stage_transition.has_value() ||
            network::IsInputLockstepCatchupBlocking(*shell->state)) {
            gubsy_add_alert(shell->runtime, "Waiting for host state", GubsyAlertSeverity::Info);
            return;
        }
        shell->state->pause = false;
        shell->state->game_over = false;
        if (shell->state->mode != Mode::Playing) {
            shell->state->scene_frame = 0;
            shell->state->SetMode(Mode::Playing);
        }
        gubsy_add_alert(shell->runtime, "Entering hosted game", GubsyAlertSeverity::Success);
        gubsy_clear_menu_stack(shell->runtime);
        SuppressGameplayInputAfterMenu(*shell->state);
        return;
    }

    ApplyLobbyConfigToSplonks(*shell, gubsy_get_lobby_state(shell->runtime),
                              shell->state->net_session.role == network::NetRole::Offline);
    if (shell->state->net_session.role == network::NetRole::Host) {
        std::string status;
        if (!network::RequestRunStart(*shell->state, MakeRandomStageSeed(), &status)) {
            if (!status.empty()) {
                gubsy_add_alert(shell->runtime, status.c_str(), GubsyAlertSeverity::Error);
            }
            return;
        }
        if (!status.empty()) {
            gubsy_add_alert(shell->runtime, status.c_str(), GubsyAlertSeverity::Success);
        }
        gubsy_clear_menu_stack(shell->runtime);
        SuppressGameplayInputAfterMenu(*shell->state);
        return;
    }
    QueueStageTransition(
        *shell->state,
        StageTransitionTarget{
            .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_1"),
            .preserve_player_state = false,
            .seed = MakeRandomStageSeed(),
        }
    );
    shell->state->scene_frame = 0;
    shell->state->SetMode(Mode::StageTransition);
}

void SuppressGameplayInputAfterMenu(State& state) {
    state.suppress_gameplay_input = false;
    state.gameplay_input_suppression_frames = 2;
    state.playing_input_snapshot = PlayingInputSnapshot::New();
    state.immediate_playing_inputs = PlayingInputs::New();
    state.previous_immediate_playing_input_snapshot = state.playing_input_snapshot;
}

void ResumeSplonksFromGubsy(void* user_data, std::int32_t) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr)
        return;
    gubsy_close_in_game_menu(shell->runtime);
    shell->state->pause = false;
    SuppressGameplayInputAfterMenu(*shell->state);
}

void RestartSplonksFromGubsy(void* user_data, std::int32_t) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr)
        return;
    if (shell->state->net_session.role != network::NetRole::Offline) {
        std::string status;
        (void)network::RequestRunRestart(*shell->state, &status);
        if (!status.empty())
            std::cerr << status << '\n';
        gubsy_close_in_game_menu(shell->runtime);
        shell->state->pause = false;
        SuppressGameplayInputAfterMenu(*shell->state);
        return;
    }
    gubsy_close_in_game_menu(shell->runtime);
    QueueStageTransition(
        *shell->state,
        StageTransitionTarget{
            .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_1"),
            .preserve_player_state = false,
            .seed = MakeRandomStageSeed(),
        }
    );
    shell->state->scene_frame = 0;
    shell->state->game_over = false;
    shell->state->pause = false;
    shell->state->SetMode(Mode::StageTransition);
    SuppressGameplayInputAfterMenu(*shell->state);
}

void QuitRunToMainMenuFromGubsy(void* user_data, std::int32_t) {
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr)
        return;
    std::string status;
    if (gubsy_get_lobby_state(shell->runtime).online) {
        (void)gubsy_leave_lobby_room(shell->runtime, status);
        if (!status.empty())
            std::cerr << status << '\n';
    }
    network::DisconnectSession(*shell->state, &status);
    if (!status.empty())
        std::cerr << status << '\n';
    shell->state->pause = false;
    shell->state->SetMode(Mode::Title);
    SuppressGameplayInputAfterMenu(*shell->state);
    (void)gubsy_show_main_menu(shell->runtime);
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
    const std::vector<std::string> addresses = network::GetLocalLanIpv4Addresses();
    if (!addresses.empty())
        return addresses.front();
    return "127.0.0.1";
}

bool RealnetPunchForced() {
    if (const char* value = std::getenv("SPLONKS_REALNET_FORCE_NAT_PUNCH")) {
        return *value != '\0' && std::string(value) != "0";
    }
    return false;
}

std::uint64_t DirectJoinTimeoutMs() {
    if (const char* value = std::getenv("SPLONKS_REALNET_DIRECT_TIMEOUT_MS")) {
        if (*value != '\0') {
            try {
                const long long parsed = std::stoll(value);
                if (parsed > 0)
                    return static_cast<std::uint64_t>(parsed);
            } catch (...) {
                return kDirectJoinTimeoutMs;
            }
        }
    }
    return kDirectJoinTimeoutMs;
}

bool ParseHttpEndpoint(const std::string& url, std::string& host, std::uint16_t& port) {
    constexpr const char* kPrefix = "http://";
    if (url.rfind(kPrefix, 0) != 0)
        return false;
    std::string work = url.substr(std::char_traits<char>::length(kPrefix));
    const std::size_t slash = work.find('/');
    if (slash != std::string::npos)
        work = work.substr(0, slash);
    const std::size_t colon = work.rfind(':');
    host = colon == std::string::npos ? work : work.substr(0, colon);
    int parsed_port = 8788;
    if (colon != std::string::npos) {
        try {
            parsed_port = std::stoi(work.substr(colon + 1));
        } catch (...) {
            return false;
        }
    }
    if (host.empty() || parsed_port <= 0 || parsed_port >= 65535)
        return false;
    port = static_cast<std::uint16_t>(parsed_port);
    return true;
}

bool RealnetRendezvousEndpoint(const GubsyLobbyState& lobby, network::NetEndpoint& endpoint) {
    std::string host;
    std::uint16_t http_port = 0;
    if (!ParseHttpEndpoint(lobby.room_server_url, host, http_port))
        return false;
    int rendezvous_port = static_cast<int>(http_port) + 1;
    if (const char* value = std::getenv("SPLONKS_REALNET_RENDEZVOUS_PORT")) {
        if (*value != '\0') {
            try {
                rendezvous_port = std::stoi(value);
            } catch (...) {
                return false;
            }
        }
    }
    if (rendezvous_port <= 0 || rendezvous_port > 65535)
        return false;
    endpoint = network::NetEndpoint{.address = host,
                                    .port = static_cast<std::uint16_t>(rendezvous_port)};
    return true;
}

GubsyLobbyHostResult HostSplonksFromGubsy(void* user_data, const GubsyLobbyState& lobby,
                                          std::uint16_t port) {
    GubsyLobbyHostResult result;
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr)
        return result;
    shell->direct_join_pending = false;
    shell->direct_join_endpoint.clear();
    shell->direct_join_started_ms = 0;
    shell->realnet_fallback_started = false;
    shell->realnet_host_room_code.clear();
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
    network::NetEndpoint rendezvous_endpoint;
    const std::string realnet_room_code = !lobby.pending_join_room.room_code.empty()
        ? lobby.pending_join_room.room_code
        : lobby.room_code;
    const bool force_realnet = RealnetPunchForced() &&
                               !realnet_room_code.empty() &&
                               !lobby.pending_join_attempt_id.empty() &&
                               !lobby.pending_punch_secret.empty() &&
                               RealnetRendezvousEndpoint(lobby, rendezvous_endpoint);
    if (force_realnet) {
        result.ok = network::JoinHostSessionViaRealnetPunch(*shell->state,
                                                            rendezvous_endpoint,
                                                            realnet_room_code,
                                                            lobby.pending_join_attempt_id,
                                                            lobby.pending_punch_secret,
                                                            {},
                                                            &result.status);
    } else {
        result.ok = network::JoinHostSession(*shell->state, host, port, &result.status);
    }
    if (result.ok) {
        result.pending = true;
        shell->direct_join_pending = true;
        shell->realnet_fallback_started = force_realnet;
        shell->direct_join_endpoint = force_realnet
            ? "Realnet NAT punch " + network::EndpointToString(rendezvous_endpoint)
            : std::string(host) + ":" + std::to_string(port);
        shell->direct_join_started_ms = SDL_GetTicks();
    }
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

bool DirectJoinAccepted(const State& state) {
    return state.net_session.role == network::NetRole::Peer &&
           state.net_transport &&
           !state.net_transport->join_request_pending;
}

bool DirectPeerReadyToPlay(const State& state) {
    return state.net_session.role == network::NetRole::Peer &&
           state.net_transport &&
           !state.net_transport->join_request_pending &&
           !network::IsInputLockstepCatchupBlocking(state) &&
           !state.net_session.quest_id.empty() &&
           !state.net_session.quest_stage_id.empty();
}

void EnterHostedGameFromStaleTransition(Shell& shell) {
    if (shell.state == nullptr)
        return;
    State& state = *shell.state;
    if (state.mode != Mode::StageTransition || state.pending_stage_transition.has_value())
        return;
    state.pause = false;
    state.game_over = false;
    state.scene_frame = 0;
    state.SetMode(Mode::Playing);
    gubsy_add_alert(shell.runtime, "Entering hosted game", GubsyAlertSeverity::Success);
    gubsy_clear_menu_stack(shell.runtime);
    SuppressGameplayInputAfterMenu(state);
}

void SyncLobbySessionPhase(Shell& shell) {
    if (shell.state == nullptr)
        return;

    const GubsyLobbyState& lobby = gubsy_get_lobby_state(shell.runtime);
    if (!lobby.online) {
        shell.joined_room_host_in_game = false;
        shell.realnet_host_room_code.clear();
        (void)gubsy_set_lobby_player_roster_locked(shell.runtime, false);
        return;
    }

    State& state = *shell.state;
    if (lobby.is_host && !lobby.room_code.empty() && !lobby.host_secret.empty() &&
        shell.realnet_host_room_code != lobby.room_code) {
        network::NetEndpoint rendezvous_endpoint;
        if (RealnetRendezvousEndpoint(lobby, rendezvous_endpoint)) {
            std::string status;
            if (network::ConfigureHostRealnetPunch(state,
                                                   rendezvous_endpoint,
                                                   lobby.room_code,
                                                   lobby.host_secret,
                                                   &status)) {
                shell.realnet_host_room_code = lobby.room_code;
                std::cerr << status << '\n';
            }
        }
    }
    if (!lobby.room_code.empty() && !lobby.is_host) {
        if (state.net_session.role != network::NetRole::Peer) {
            shell.joined_room_host_in_game = false;
            return;
        }
        shell.joined_room_host_in_game =
            shell.joined_room_host_in_game || session_contract_is_in_game(lobby.contract);
        const bool client_ready = shell.joined_room_host_in_game && DirectPeerReadyToPlay(state);
        if (client_ready) {
            EnterHostedGameFromStaleTransition(shell);
        } else if (shell.joined_room_host_in_game &&
                   state.net_transport &&
                   !state.net_transport->join_request_pending &&
                   state.mode != Mode::StageTransition &&
                   state.mode != Mode::Playing) {
            state.pending_stage_transition.reset();
            state.scene_frame = 0;
            state.SetMode(Mode::StageTransition);
            gubsy_clear_menu_stack(shell.runtime);
        }
        (void)gubsy_set_lobby_player_roster_locked(shell.runtime,
                                                   shell.joined_room_host_in_game);
        return;
    }
    shell.joined_room_host_in_game = false;

    bool in_game = false;
    if (state.net_session.role == network::NetRole::Host) {
        in_game = session_contract_is_in_game(lobby.contract) ||
                  state.mode == Mode::Playing ||
                  state.mode == Mode::StageTransition ||
                  state.mode == Mode::GameOver ||
                  state.pending_stage_transition.has_value();
    } else if (state.net_session.role == network::NetRole::Peer) {
        in_game = DirectPeerReadyToPlay(state);
        if (in_game)
            EnterHostedGameFromStaleTransition(shell);
    }
    (void)gubsy_set_lobby_session_phase(shell.runtime, in_game ? "in_game" : "lobby");
    (void)gubsy_set_lobby_player_roster_locked(shell.runtime, in_game);
}

void SyncDirectJoinStatus(Shell& shell) {
    if (!shell.direct_join_pending || shell.state == nullptr)
        return;

    if (DirectJoinAccepted(*shell.state)) {
        std::string message = "Joined direct " + shell.direct_join_endpoint;
        gubsy_confirm_lobby_direct_join(shell.runtime, message);
        if (gubsy_get_lobby_state(shell.runtime).online)
            (void)gubsy_show_lobby_menu(shell.runtime);
        shell.direct_join_pending = false;
        shell.realnet_fallback_started = false;
        shell.direct_join_endpoint.clear();
        shell.direct_join_started_ms = 0;
        return;
    }

    const std::uint64_t now_ms = SDL_GetTicks();
    if (now_ms - shell.direct_join_started_ms < DirectJoinTimeoutMs())
        return;

    const GubsyLobbyState& lobby = gubsy_get_lobby_state(shell.runtime);
    network::NetEndpoint rendezvous_endpoint;
    if (!shell.realnet_fallback_started &&
        lobby.room_join_pending &&
        !lobby.pending_join_room.room_code.empty() &&
        !lobby.pending_join_attempt_id.empty() &&
        !lobby.pending_punch_secret.empty() &&
        RealnetRendezvousEndpoint(lobby, rendezvous_endpoint)) {
        std::string status;
        network::DisconnectSession(*shell.state, &status);
        const bool started = network::JoinHostSessionViaRealnetPunch(*shell.state,
                                                                     rendezvous_endpoint,
                                                                     lobby.pending_join_room.room_code,
                                                                     lobby.pending_join_attempt_id,
                                                                     lobby.pending_punch_secret,
                                                                     {},
                                                                     &status);
        if (started) {
            shell.realnet_fallback_started = true;
            shell.direct_join_endpoint = "Realnet NAT punch " +
                                         network::EndpointToString(rendezvous_endpoint);
            shell.direct_join_started_ms = SDL_GetTicks();
            std::cerr << status << '\n';
            return;
        }
    }

    std::string status;
    network::DisconnectSession(*shell.state, &status);
    std::string message = "No server found at " + shell.direct_join_endpoint;
    gubsy_fail_lobby_direct_join(shell.runtime, message);
    shell.direct_join_pending = false;
    shell.realnet_fallback_started = false;
    shell.direct_join_endpoint.clear();
    shell.direct_join_started_ms = 0;
}

bool ParseDirectMemberEndpoint(const std::string& member_id, std::string& address,
                               std::uint16_t& port) {
    constexpr const char* kPrefix = "direct:";
    if (member_id.rfind(kPrefix, 0) != 0) {
        return false;
    }
    const std::string endpoint = member_id.substr(std::char_traits<char>::length(kPrefix));
    const std::size_t colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon + 1 >= endpoint.size()) {
        return false;
    }
    address = endpoint.substr(0, colon);
    if (address.empty() || address == "player") {
        return false;
    }
    try {
        const int parsed = std::stoi(endpoint.substr(colon + 1));
        if (parsed <= 0 || parsed > 65535) {
            return false;
        }
        port = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseDirectMemberPlayerId(const std::string& member_id, PlayerId& player_id) {
    constexpr const char* kPrefix = "direct:player:";
    if (member_id.rfind(kPrefix, 0) != 0) {
        return false;
    }
    try {
        const int parsed =
            std::stoi(member_id.substr(std::char_traits<char>::length(kPrefix)));
        if (parsed <= 0) {
            return false;
        }
        player_id = static_cast<PlayerId>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

GubsyLobbyKickResult KickDirectSplonksMemberFromGubsy(void* user_data,
                                                      const GubsyLobbyState&,
                                                      const MatchmakingMember& member) {
    GubsyLobbyKickResult result;
    auto* shell = static_cast<Shell*>(user_data);
    if (shell == nullptr || shell->state == nullptr) {
        result.status = "Direct kick is not available.";
        return result;
    }

    std::string address;
    std::uint16_t port = 0;
    if (ParseDirectMemberEndpoint(member.member_id, address, port)) {
        result.ok = network::KickRemoteEndpoint(*shell->state, address, port, &result.status);
    } else {
        PlayerId player_id = kInvalidPlayerId;
        if (ParseDirectMemberPlayerId(member.member_id, player_id)) {
            result.ok = network::KickRemotePlayer(*shell->state, player_id, &result.status);
        } else {
            result.status = "Cannot identify direct player.";
        }
    }
    if (!result.status.empty())
        std::cerr << result.status << '\n';
    return result;
}

MenuInputState BuildGubsyMenuInput(const MenuInputs& inputs, bool text_edit_active) {
    (void)text_edit_active;
    MenuInputState result{};
    result.up = inputs.up.down;
    result.down = inputs.down.down;
    result.left = inputs.left.down;
    result.right = inputs.right.down;
    result.select = inputs.confirm.down;
    result.back = inputs.back.down;
    result.page_prev = inputs.page_prev.down;
    result.page_next = inputs.page_next.down;
    return result;
}

MenuInputState BuildFrameMenuInput(Shell& shell, const State& state) {
    MenuInputState input = BuildGubsyMenuInput(state.menu_inputs, TextEditActive(shell));
    if (shell.block_next_menu_input) {
        shell.block_next_menu_input = false;
        return {};
    }
    if (shell.block_menu_input_until_release) {
        const bool any_open_input_held = input.back || input.select;
        shell.block_menu_input_until_release = any_open_input_held;
        if (any_open_input_held)
            return {};
    }
    return input;
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
    EnsureDefaultBinds(shell.runtime);

    GubsyMainMenuCommands commands{};
    commands.start_game = gubsy_register_menu_command(shell.runtime, StartSplonksFromGubsy, &shell);
    commands.quit = gubsy_register_menu_command(shell.runtime, QuitSplonksFromGubsy, &state);
    gubsy_set_main_menu_commands(shell.runtime, commands);

    GubsyInGameMenuCommands in_game_commands{};
    shell.in_game_resume_command =
        gubsy_register_menu_command(shell.runtime, ResumeSplonksFromGubsy, &shell);
    shell.in_game_restart_run_command =
        gubsy_register_menu_command(shell.runtime, RestartSplonksFromGubsy, &shell);
    shell.in_game_quit_to_main_menu_command =
        gubsy_register_menu_command(shell.runtime, QuitRunToMainMenuFromGubsy, &shell);
    in_game_commands.resume = shell.in_game_resume_command;
    in_game_commands.restart_run = shell.in_game_restart_run_command;
    in_game_commands.quit_to_main_menu = shell.in_game_quit_to_main_menu_command;
    gubsy_set_in_game_menu_commands(shell.runtime, in_game_commands);

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
    lobby_commands.kick_direct_member = KickDirectSplonksMemberFromGubsy;
    lobby_commands.kick_direct_member_user_data = &shell;
    gubsy_set_lobby_commands(shell.runtime, lobby_commands);
    return gubsy_show_main_menu(shell.runtime);
}

void SyncInGameMenuCommands(Shell& shell) {
    if (shell.state == nullptr)
        return;
    GubsyInGameMenuCommands commands{};
    commands.resume = shell.in_game_resume_command;
    commands.restart_run = shell.state->net_session.role == network::NetRole::Peer
        ? kMenuIdInvalid
        : shell.in_game_restart_run_command;
    commands.quit_to_main_menu = shell.in_game_quit_to_main_menu_command;
    gubsy_set_in_game_menu_commands(shell.runtime, commands);
}

const network::NetPeerState* FindPeerStateForPlayer(const State& state, PlayerId player_id) {
    for (const network::NetPeerState& peer : state.net_session.peers) {
        if (peer.player_id == player_id) {
            return &peer;
        }
    }
    return nullptr;
}

std::vector<MatchmakingMember> BuildNetworkRemoteMembers(const State& state) {
    std::vector<MatchmakingMember> members;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Remote ||
            slot.player_id == kInvalidPlayerId) {
            continue;
        }

        const network::NetPeerState* peer = FindPeerStateForPlayer(state, slot.player_id);
        MatchmakingMember member;
        member.display_name = !slot.display_name.empty()
            ? slot.display_name
            : "Remote " + std::to_string(slot.player_id);
        member.is_host = slot.player_id == state.net_session.host_player_id;
        member.member_id = "direct:player:" + std::to_string(slot.player_id);
        if (peer != nullptr && !peer->endpoint_address.empty() && peer->endpoint_port != 0) {
            member.client_label = peer->endpoint_address + ":" + std::to_string(peer->endpoint_port);
        }
        members.push_back(std::move(member));
    }
    return members;
}

void SyncDirectNetworkMembers(Shell& shell) {
    if (shell.state == nullptr) {
        return;
    }

    SyncLobbySessionPhase(shell);

    const GubsyLobbyState& lobby = gubsy_get_lobby_state(shell.runtime);
    if (!lobby.online) {
        return;
    }

    const State& state = *shell.state;
    if (state.net_session.role == network::NetRole::Offline) {
        gubsy_set_lobby_direct_members(shell.runtime, {});
        return;
    }

    std::vector<MatchmakingMember> members;
    if (!lobby.room_code.empty()) {
        if (!lobby.is_host) {
            return;
        }
    }
    std::vector<MatchmakingMember> network_members = BuildNetworkRemoteMembers(state);
    members.insert(members.end(),
                   std::make_move_iterator(network_members.begin()),
                   std::make_move_iterator(network_members.end()));

    gubsy_set_lobby_direct_members(shell.runtime, members);
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

bool InitHeadless(Shell& shell, State& state) {
    if (!init_gubsy_runtime(shell.runtime, BuildGubsyConfig()))
        return false;
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
    if (shell.direct_join_pending) {
        SyncDirectJoinStatus(shell);
    }
    SyncDirectNetworkMembers(shell);
    gubsy_begin_debug_frame(shell.runtime, dt);
}

void UpdateRuntime(Shell& shell, float dt) {
    if (shell.direct_join_pending) {
        SyncDirectJoinStatus(shell);
    }
    SyncDirectNetworkMembers(shell);
    gubsy_update_runtime(shell.runtime, dt);
}

bool OpenInGameMenu(Shell& shell) {
    if (shell.state == nullptr)
        return false;
    SyncInGameMenuCommands(shell);
    if (!gubsy_open_in_game_menu(shell.runtime))
        return false;
    shell.block_next_menu_input = true;
    shell.block_menu_input_until_release = true;
    shell.state->suppress_gameplay_input = true;
    shell.state->gameplay_input_suppression_frames = 1;
    shell.state->pause = shell.state->net_session.role == network::NetRole::Offline;
    return true;
}

void CloseInGameMenu(Shell& shell) {
    gubsy_close_in_game_menu(shell.runtime);
    if (shell.state == nullptr)
        return;
    shell.state->pause = false;
    SuppressGameplayInputAfterMenu(*shell.state);
}

bool InGameMenuOpen(Shell& shell) {
    return gubsy_in_game_menu_open(shell.runtime);
}

void UpdateMenu(Shell& shell, const State& state, float dt, int screen_width, int screen_height) {
    if (shell.direct_join_pending) {
        SyncDirectJoinStatus(shell);
    }
    SyncDirectNetworkMembers(shell);
    gubsy_set_menu_input(shell.runtime, BuildFrameMenuInput(shell, state));
    gubsy_update_menu(shell.runtime, dt, screen_width, screen_height);
}

void RenderMenu(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height) {
    gubsy_render_menu(shell.runtime, renderer, screen_width, screen_height);
}

void RenderAlerts(Shell& shell, SDL_Renderer* renderer, int screen_width) {
    gubsy_render_alerts(shell.runtime, renderer, screen_width);
}

void UpdateTitleMenu(Shell& shell, State& state, Graphics& graphics, float dt, int screen_width,
                     int screen_height) {
    (void)graphics;
    if (shell.direct_join_pending) {
        SyncDirectJoinStatus(shell);
    }
    UpdateMenu(shell, state, dt, screen_width, screen_height);
}

void RenderTitleMenu(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height) {
    RenderMenu(shell, renderer, screen_width, screen_height);
}

void RenderDebug(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height) {
    gubsy_render_debug(shell.runtime, renderer, screen_width, screen_height);
}

void ShutdownDebug(Shell& shell) {
    gubsy_shutdown_debug(shell.runtime);
}

void AddAlert(Shell& shell, const std::string& text, GubsyAlertSeverity severity) {
    gubsy_add_alert(shell.runtime, text, severity);
}

void Shutdown(Shell& shell) {
    gubsy_shutdown_debug(shell.runtime);
    cleanup_gubsy_runtime(shell.runtime);
}

} // namespace splonks::gubsy_shell

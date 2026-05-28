#include "gubsy_shell_smoke.hpp"

#include "gubsy_shell.hpp"
#include "network/net_lobby.hpp"

#include <cstdint>
#include <gubsy/runtime.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace splonks {
namespace {

bool ParseEndpoint(const std::string& endpoint, std::string& host, std::uint16_t& port) {
    const std::size_t colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon + 1 >= endpoint.size())
        return false;
    host = endpoint.substr(0, colon);
    try {
        const int parsed = std::stoi(endpoint.substr(colon + 1));
        if (host.empty() || parsed <= 0 || parsed > 65535)
            return false;
        port = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

struct SmokeMatchmaking final : IMatchmaking {
    MatchmakingRoom room;
    bool has_room = false;
    bool create_called = false;
    bool join_called = false;
    bool leave_called = false;

    bool create_room(const std::string&, const MatchmakingRoom& created_room,
                     MatchmakingCreateResult& out, std::string&) override {
        create_called = true;
        room = created_room;
        room.room_code = "SMOKE1";
        has_room = true;
        out.room_code = room.room_code;
        out.host_secret = "host-secret";
        out.member_id = "host-member";
        return true;
    }

    bool join_room(const std::string&, const std::string& room_code, const std::string&,
                   std::string& member_id_out, std::string& err) override {
        join_called = true;
        if (!has_room || room_code != room.room_code) {
            err = "room not found";
            return false;
        }
        member_id_out = "guest-member";
        return true;
    }

    bool leave_room(const std::string&, const std::string& room_code, const std::string&,
                    const std::string&, std::string& err) override {
        leave_called = true;
        if (room_code != room.room_code) {
            err = "room not found";
            return false;
        }
        return true;
    }

    bool remove_member(const std::string&, const std::string&, const std::string&,
                       const std::string&, std::string&) override {
        return true;
    }

    bool heartbeat_room(const std::string&, const std::string&, const std::string&,
                        const std::string&, const std::string&, const MatchmakingRoom*,
                        std::string&) override {
        return true;
    }

    bool fetch_room(const std::string&, const std::string& room_code, MatchmakingRoom& out,
                    std::string& err) override {
        if (!has_room || room_code != room.room_code) {
            err = "room not found";
            return false;
        }
        out = room;
        return true;
    }

    bool list_rooms(const std::string&, std::vector<MatchmakingRoom>& out, std::string&) override {
        out.clear();
        if (has_room)
            out.push_back(room);
        return true;
    }
};

void StepMenu(gubsy_shell::Shell& shell, State& state, const MenuInputs& inputs = MenuInputs{}) {
    state.menu_inputs = inputs;
    gubsy_shell::UpdateMenu(shell, state, 0.016F, 1280, 720);
}

void PressDown(gubsy_shell::Shell& shell, State& state) {
    MenuInputs inputs = MenuInputs::New();
    inputs.down.down = true;
    StepMenu(shell, state, inputs);
    StepMenu(shell, state);
}

void PressSelect(gubsy_shell::Shell& shell, State& state) {
    MenuInputs inputs = MenuInputs::New();
    inputs.confirm.down = true;
    StepMenu(shell, state, inputs);
    StepMenu(shell, state);
}

void PressRight(gubsy_shell::Shell& shell, State& state) {
    MenuInputs inputs = MenuInputs::New();
    inputs.right.down = true;
    StepMenu(shell, state, inputs);
    StepMenu(shell, state);
}

bool CheckOfflineStart() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless failed\n";
        return false;
    }

    std::string message;
    const bool started = gubsy_start_lobby_game(shell.runtime, message);
    gubsy_shell::Shutdown(shell);
    if (!started) {
        std::cerr << "Gubsy shell smoke failed: " << message << '\n';
        return false;
    }
    if (state.mode != Mode::StageTransition) {
        std::cerr << "Gubsy shell smoke failed: lobby start did not enter stage transition\n";
        return false;
    }
    if (!state.pending_stage_transition.has_value() ||
        state.pending_stage_transition->destination.kind != StageLoadTargetKind::QuestStage ||
        std::string_view(state.pending_stage_transition->destination.quest_id.data()) !=
            "classic" ||
        std::string_view(state.pending_stage_transition->destination.quest_stage_id.data()) !=
            "classic_mines_1") {
        std::cerr << "Gubsy shell smoke failed: lobby start did not queue Mines 1\n";
        return false;
    }
    if (state.players.FindPrimaryLocal() == nullptr) {
        std::cerr << "Gubsy shell smoke failed: missing primary local player\n";
        return false;
    }
    if (state.multiplayer_respawn_mode != MultiplayerRespawnMode::GenerousNextLevel) {
        std::cerr << "Gubsy shell smoke failed: lobby config was not applied\n";
        return false;
    }
    return true;
}

bool CheckInGameMenuShell() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for in-game menu failed\n";
        return false;
    }

    state.SetMode(Mode::Playing);
    if (!gubsy_shell::OpenInGameMenu(shell)) {
        std::cerr << "Gubsy shell smoke failed: in-game menu did not open\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!gubsy_shell::InGameMenuOpen(shell) || !state.pause || !state.suppress_gameplay_input) {
        std::cerr << "Gubsy shell smoke failed: in-game menu did not pause offline play\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    MenuInputs held_back = MenuInputs::New();
    held_back.back.down = true;
    StepMenu(shell, state, held_back);
    StepMenu(shell, state, held_back);
    if (!gubsy_shell::InGameMenuOpen(shell)) {
        std::cerr << "Gubsy shell smoke failed: opening back input closed menu immediately\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    StepMenu(shell, state);

    gubsy_shell::CloseInGameMenu(shell);
    if (gubsy_shell::InGameMenuOpen(shell) || state.pause ||
        state.gameplay_input_suppression_frames <= 0) {
        std::cerr << "Gubsy shell smoke failed: in-game menu did not close cleanly\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckPeerInputMapping() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for peer input mapping failed\n";
        return false;
    }

    state.net_session.role = network::NetRole::Peer;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.local_player_id = 2;
    state.players.EnsureLocalPlayer(2, "Player 2", true);

    gubsy_shell::ApplyLobbyGameplayInput(shell);
    if (!state.use_external_local_input_frames ||
        state.external_local_input_frames.size() < 2) {
        std::cerr << "Gubsy shell smoke failed: peer input did not map to assigned player id\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckInGameRestartCommand() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for restart failed\n";
        return false;
    }

    state.SetMode(Mode::Playing);
    if (!gubsy_shell::OpenInGameMenu(shell)) {
        std::cerr << "Gubsy shell smoke failed: restart menu did not open\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    StepMenu(shell, state);
    PressDown(shell, state);
    PressDown(shell, state);
    PressSelect(shell, state);

    const bool queued_mines_1 =
        state.pending_stage_transition.has_value() &&
        state.pending_stage_transition->destination.kind == StageLoadTargetKind::QuestStage &&
        std::string_view(state.pending_stage_transition->destination.quest_id.data()) ==
            "classic" &&
        std::string_view(state.pending_stage_transition->destination.quest_stage_id.data()) ==
            "classic_mines_1";
    if (gubsy_shell::InGameMenuOpen(shell) || state.mode != Mode::StageTransition ||
        !queued_mines_1 || state.pause) {
        std::cerr << "Gubsy shell smoke failed: restart command did not queue run restart\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckNetworkRestartCommandDoesNotDesync() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for network restart failed\n";
        return false;
    }

    state.SetMode(Mode::Playing);
    std::string status;
    if (!network::StartHostSession(state, 0, &status)) {
        std::cerr << "Gubsy shell smoke failed: host session for network restart failed: "
                  << status << '\n';
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!gubsy_shell::OpenInGameMenu(shell)) {
        std::cerr << "Gubsy shell smoke failed: network restart menu did not open\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    StepMenu(shell, state);
    PressDown(shell, state);
    PressDown(shell, state);
    PressSelect(shell, state);

    if (gubsy_shell::InGameMenuOpen(shell) || state.mode != Mode::Playing ||
        state.pending_stage_transition.has_value() || !state.net_session.run_restart_pending ||
        state.net_session.role != network::NetRole::Host || state.pause) {
        std::cerr << "Gubsy shell smoke failed: network restart was not scheduled safely\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckInGameQuitCommand() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for quit failed\n";
        return false;
    }

    state.SetMode(Mode::Playing);
    std::string status;
    if (!network::StartHostSession(state, 0, &status)) {
        std::cerr << "Gubsy shell smoke failed: host session for quit failed: " << status << '\n';
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!gubsy_shell::OpenInGameMenu(shell)) {
        std::cerr << "Gubsy shell smoke failed: quit menu did not open\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    StepMenu(shell, state);
    PressDown(shell, state);
    PressDown(shell, state);
    PressDown(shell, state);
    PressSelect(shell, state);

    if (gubsy_shell::InGameMenuOpen(shell) || state.mode != Mode::Title ||
        state.net_session.role != network::NetRole::Offline || state.pause) {
        std::cerr << "Gubsy shell smoke failed: quit command did not return to title offline\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckHostJoin() {
    SmokeMatchmaking matchmaking;
    std::string message;

    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Gubsy shell smoke failed: host InitHeadless failed\n";
        return false;
    }
    gubsy_set_lobby_matchmaking_backend(host_shell.runtime, &matchmaking);
    if (!gubsy_host_lobby_direct(host_shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell smoke failed: direct host failed: " << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (host_state.net_session.role != network::NetRole::Host) {
        std::cerr << "Gubsy shell smoke failed: direct host callback was not used\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    const GubsyLobbyState& direct_lobby = gubsy_get_lobby_state(host_shell.runtime);
    std::string direct_host;
    std::uint16_t direct_port = 0;
    if (!ParseEndpoint(direct_lobby.advertised_endpoint, direct_host, direct_port)) {
        std::cerr << "Gubsy shell smoke failed: direct host endpoint invalid\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    State direct_guest_state = State::New();
    gubsy_shell::Shell direct_guest_shell;
    if (!gubsy_shell::InitHeadless(direct_guest_shell, direct_guest_state)) {
        std::cerr << "Gubsy shell smoke failed: direct guest InitHeadless failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!gubsy_join_lobby_direct(direct_guest_shell.runtime, direct_host, direct_port, message)) {
        std::cerr << "Gubsy shell smoke failed: direct join failed: " << message << '\n';
        gubsy_shell::Shutdown(direct_guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (direct_guest_state.net_session.role != network::NetRole::Peer) {
        std::cerr << "Gubsy shell smoke failed: direct join callback was not used\n";
        gubsy_shell::Shutdown(direct_guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    (void)gubsy_leave_lobby_room(direct_guest_shell.runtime, message);
    (void)gubsy_leave_lobby_room(host_shell.runtime, message);
    gubsy_shell::Shutdown(direct_guest_shell);

    if (!gubsy_host_lobby_room(host_shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell smoke failed: host room failed: " << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!matchmaking.create_called || host_state.net_session.role != network::NetRole::Host) {
        std::cerr << "Gubsy shell smoke failed: host callback/backend was not used\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    State guest_state = State::New();
    gubsy_shell::Shell guest_shell;
    if (!gubsy_shell::InitHeadless(guest_shell, guest_state)) {
        std::cerr << "Gubsy shell smoke failed: guest InitHeadless failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    gubsy_set_lobby_matchmaking_backend(guest_shell.runtime, &matchmaking);
    if (!gubsy_join_lobby_room_code(guest_shell.runtime, matchmaking.room.room_code, message)) {
        std::cerr << "Gubsy shell smoke failed: join room failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!matchmaking.join_called || guest_state.net_session.role != network::NetRole::Peer) {
        std::cerr << "Gubsy shell smoke failed: join callback/backend was not used\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (guest_state.multiplayer_respawn_mode != MultiplayerRespawnMode::GenerousNextLevel) {
        std::cerr << "Gubsy shell smoke failed: remote lobby config was not applied\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    (void)gubsy_leave_lobby_room(guest_shell.runtime, message);
    (void)gubsy_leave_lobby_room(host_shell.runtime, message);
    gubsy_shell::Shutdown(guest_shell);
    gubsy_shell::Shutdown(host_shell);
    return true;
}

bool CheckDirectHostJoinViaMenu() {
    std::string message;
    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Gubsy shell smoke failed: direct menu host InitHeadless failed\n";
        return false;
    }

    if (!gubsy_push_menu_screen(host_shell.runtime, MenuScreenID::LOBBY_HOST_SETUP)) {
        std::cerr << "Gubsy shell smoke failed: direct host menu screen missing\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    StepMenu(host_shell, host_state);
    PressDown(host_shell, host_state);
    PressDown(host_shell, host_state);
    PressDown(host_shell, host_state);
    PressDown(host_shell, host_state);
    PressDown(host_shell, host_state);
    PressRight(host_shell, host_state);
    PressSelect(host_shell, host_state);
    if (host_state.net_session.role != network::NetRole::Host) {
        const GubsyLobbyState& lobby = gubsy_get_lobby_state(host_shell.runtime);
        std::cerr << "Gubsy shell smoke failed: direct host menu did not start hosting: "
                  << lobby.status_message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    State guest_state = State::New();
    gubsy_shell::Shell guest_shell;
    if (!gubsy_shell::InitHeadless(guest_shell, guest_state)) {
        std::cerr << "Gubsy shell smoke failed: direct menu guest InitHeadless failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    if (!gubsy_push_menu_screen(guest_shell.runtime, MenuScreenID::LOBBY_SERVER_BROWSER)) {
        std::cerr << "Gubsy shell smoke failed: direct join menu screen missing\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    StepMenu(guest_shell, guest_state);
    PressDown(guest_shell, guest_state);
    PressDown(guest_shell, guest_state);
    PressSelect(guest_shell, guest_state);
    if (guest_state.net_session.role != network::NetRole::Peer) {
        const GubsyLobbyState& lobby = gubsy_get_lobby_state(guest_shell.runtime);
        std::cerr << "Gubsy shell smoke failed: direct join menu did not connect: "
                  << lobby.status_message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    (void)gubsy_leave_lobby_room(guest_shell.runtime, message);
    (void)gubsy_leave_lobby_room(host_shell.runtime, message);
    gubsy_shell::Shutdown(guest_shell);
    gubsy_shell::Shutdown(host_shell);
    return true;
}

} // namespace

bool CheckGubsyShellSmoke() {
    try {
        return CheckOfflineStart() && CheckInGameMenuShell() && CheckInGameRestartCommand() &&
               CheckNetworkRestartCommandDoesNotDesync() && CheckPeerInputMapping() &&
               CheckInGameQuitCommand() && CheckHostJoin() && CheckDirectHostJoinViaMenu();
    } catch (const std::exception& e) {
        std::cerr << "Gubsy shell smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks

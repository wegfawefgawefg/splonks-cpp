#include "gubsy_shell_smoke.hpp"

#include "gubsy_shell.hpp"
#include "network/net_lobby.hpp"

#include <gubsy/runtime.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace splonks {
namespace {

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

} // namespace

bool CheckGubsyShellSmoke() {
    try {
        return CheckOfflineStart() && CheckInGameMenuShell() && CheckHostJoin();
    } catch (const std::exception& e) {
        std::cerr << "Gubsy shell smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks

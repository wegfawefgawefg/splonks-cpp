#include "gubsy_shell_smoke.hpp"

#include "audio.hpp"
#include "gubsy_shell.hpp"
#include "graphics.hpp"
#include "network/net_lobby.hpp"
#include "stage_progression.hpp"
#include "step.hpp"

#include <cstdint>
#include <cstdlib>
#include <gubsy/input/types.hpp>
#include <gubsy/lobby/room_matchmaking.hpp>
#include <gubsy/lobby/state.hpp>
#include <gubsy/runtime.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <SDL3/SDL_scancode.h>
#include <src/gubsy_runtime_internal.hpp>
#include <src/menu/menu_system_state.hpp>
#include <src/menu_layout_ids.hpp>
#include <algorithm>
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

std::size_t ConnectedLocalPlayerCount(const State& state) {
    std::size_t count = 0;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local) {
            ++count;
        }
    }
    return count;
}

std::size_t ConnectedLocalPlayerEntCount(const State& state) {
    std::size_t count = 0;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local &&
            slot.ent_vid.has_value()) {
            ++count;
        }
    }
    return count;
}

struct SmokeMatchmaking final : IMatchmaking {
    MatchmakingRoom room;
    bool has_room = false;
    bool create_called = false;
    bool join_attempt_called = false;
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
                   const std::string& join_token, std::string& member_id_out,
                   std::string& err) override {
        join_called = true;
        if (!has_room || room_code != room.room_code) {
            err = "room not found";
            return false;
        }
        if (join_token != "join-token") {
            err = "join token rejected";
            return false;
        }
        member_id_out = "guest-member";
        return true;
    }

    bool create_join_attempt(const std::string&, const std::string& room_code,
                             const std::string&, MatchmakingJoinAttemptResult& out,
                             std::string& err) override {
        join_attempt_called = true;
        if (!has_room || room_code != room.room_code) {
            err = "room not found";
            return false;
        }
        out.join_attempt_id = "join-attempt";
        out.join_token = "join-token";
        out.room = room;
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

bool GubsyAlertContains(const EngineState& engine, std::string_view needle) {
    return std::any_of(engine.alerts.begin(), engine.alerts.end(), [&](const Alert& alert) {
        return alert.text.find(needle) != std::string::npos;
    });
}

void PumpTitleNetwork(gubsy_shell::Shell& shell, State& state, Graphics& graphics) {
    Audio audio;
    StepSingleTick(state, audio, graphics);
    gubsy_shell::UpdateTitleMenu(shell, state, graphics, 0.016F, 1280, 720);
}

void StepHeadlessShell(gubsy_shell::Shell& shell, State& state, Graphics& graphics,
                       Audio& audio, bool hold_primary_right = false) {
    gubsy_shell::ApplyLobbyGameplayInput(shell);
    if (hold_primary_right) {
        state.playing_input_snapshot.right = true;
    }
    StepSingleTick(state, audio, graphics);
    if (state.mode == Mode::Title) {
        gubsy_shell::UpdateTitleMenu(shell, state, graphics, 0.016F, 1280, 720);
    } else if (gubsy_shell::InGameMenuOpen(shell)) {
        gubsy_shell::UpdateMenu(shell, state, 0.016F, 1280, 720);
    } else {
        gubsy_shell::UpdateRuntime(shell, 0.016F);
    }
}

template <typename Done>
bool PumpJoinUntilConfirmed(gubsy_shell::Shell& host_shell,
                            State& host_state,
                            gubsy_shell::Shell& guest_shell,
                            State& guest_state,
                            Done done) {
    Graphics graphics;
    for (int i = 0; i < 360; ++i) {
        PumpTitleNetwork(host_shell, host_state, graphics);
        PumpTitleNetwork(guest_shell, guest_state, graphics);
        if (done())
            return true;
    }
    return false;
}

const MenuWidget* GubsyWidgetBySlot(const EngineState& engine, UILayoutObjectId slot) {
    const auto& menu = menu_system_internal::runtime_state(engine);
    auto it = std::find_if(menu.cache.widgets.begin(), menu.cache.widgets.end(),
                           [&](const MenuWidget& widget) { return widget.slot == slot; });
    return it == menu.cache.widgets.end() ? nullptr : &*it;
}

SDL_FRect GubsyWidgetRectBySlot(const EngineState& engine, UILayoutObjectId slot) {
    const auto& menu = menu_system_internal::runtime_state(engine);
    for (std::size_t i = 0; i < menu.cache.widgets.size() && i < menu.cache.rects.size(); ++i) {
        if (menu.cache.widgets[i].slot == slot)
            return menu.cache.rects[i];
    }
    return SDL_FRect{};
}

bool RectHasArea(const SDL_FRect& rect) {
    return rect.w > 1.0F && rect.h > 1.0F;
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
    if (gubsy_add_lobby_local_player(shell.runtime) != 1 ||
        gubsy_add_lobby_local_player(shell.runtime) != 2) {
        std::cerr << "Gubsy shell smoke failed: offline local player setup failed\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    std::string message;
    const bool started = gubsy_start_lobby_game(shell.runtime, message);
    if (!started) {
        std::cerr << "Gubsy shell smoke failed: " << message << '\n';
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (state.mode != Mode::StageTransition) {
        std::cerr << "Gubsy shell smoke failed: lobby start did not enter stage transition\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!state.pending_stage_transition.has_value() ||
        state.pending_stage_transition->destination.kind != StageLoadTargetKind::QuestStage ||
        std::string_view(state.pending_stage_transition->destination.quest_id.data()) !=
            "classic" ||
        std::string_view(state.pending_stage_transition->destination.quest_stage_id.data()) !=
            "classic_mines_1") {
        std::cerr << "Gubsy shell smoke failed: lobby start did not queue Mines 1\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (state.players.FindPrimaryLocal() == nullptr) {
        std::cerr << "Gubsy shell smoke failed: missing primary local player\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    ApplyPendingStageTransition(state);
    if (ConnectedLocalPlayerCount(state) != 3 || ConnectedLocalPlayerEntCount(state) != 3) {
        std::cerr << "Gubsy shell smoke failed: offline start did not spawn three local players: slots="
                  << ConnectedLocalPlayerCount(state) << " ents="
                  << ConnectedLocalPlayerEntCount(state) << '\n';
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (state.multiplayer_respawn_mode != MultiplayerRespawnMode::GenerousNextLevel) {
        std::cerr << "Gubsy shell smoke failed: lobby config was not applied\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckHostStartKeepsLocalPlayers() {
    std::string message;
    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Gubsy shell smoke failed: multi-local host start InitHeadless failed\n";
        return false;
    }
    if (gubsy_add_lobby_local_player(host_shell.runtime) != 1 ||
        gubsy_add_lobby_local_player(host_shell.runtime) != 2) {
        std::cerr << "Gubsy shell smoke failed: multi-local host setup failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!gubsy_host_lobby_direct(host_shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell smoke failed: multi-local host start failed: " << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (ConnectedLocalPlayerCount(host_state) != 3 || ConnectedLocalPlayerEntCount(host_state) != 3) {
        std::cerr << "Gubsy shell smoke failed: hosted start did not keep three local players: slots="
                  << ConnectedLocalPlayerCount(host_state) << " ents="
                  << ConnectedLocalPlayerEntCount(host_state) << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    (void)gubsy_leave_lobby_room(host_shell.runtime, message);
    gubsy_shell::Shutdown(host_shell);
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

bool CheckPeerGameplayInputAfterStart() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for peer gameplay input failed\n";
        return false;
    }

    state.SetMode(Mode::Playing);
    state.net_session.role = network::NetRole::Peer;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.local_player_id = 2;
    state.players.EnsureLocalPlayer(2, "Player 2", true);
    state.suppress_gameplay_input = false;
    state.gameplay_input_suppression_frames = 0;

    EngineState& gubsy_engine = gubsy_runtime_engine(shell.runtime);
    gubsy_engine.device_state.keyboard[static_cast<std::size_t>(SDL_SCANCODE_D)] = 1;

    gubsy_shell::ApplyLobbyGameplayInput(shell);
    if (!state.use_external_local_input_frames ||
        state.external_local_input_frames.size() < 2 ||
        !state.external_local_input_frames[1].right) {
        std::cerr << "Gubsy shell smoke failed: joined-client gameplay input did not reach "
                     "assigned player 2 after start\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (state.external_local_input_frames[0].right) {
        std::cerr << "Gubsy shell smoke failed: joined-client gameplay input leaked to host "
                     "player 1\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (state.suppress_gameplay_input || state.gameplay_input_suppression_frames != 0 ||
        gubsy_shell::InGameMenuOpen(shell)) {
        std::cerr << "Gubsy shell smoke failed: gameplay input remained menu-suppressed after "
                     "start\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckPeerPlayLeavesStaleTransition() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for stale peer transition failed\n";
        return false;
    }

    state.SetMode(Mode::StageTransition);
    state.pending_stage_transition = StageTransitionTarget{
        .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_1"),
    };
    state.net_session.role = network::NetRole::Peer;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.local_player_id = 2;
    state.net_session.quest_id = "classic";
    state.net_session.quest_stage_id = "classic_mines_1";

    EngineState& engine = gubsy_runtime_engine(shell.runtime);
    engine.lobby.online = true;
    engine.lobby.is_host = false;
    engine.lobby.room_code = "ROOM1";
    engine.lobby.contract.session_phase = "in_game";
    engine.lobby.contract.realtime_endpoint = "127.0.0.1:35355";

    std::string message;
    if (!gubsy_start_lobby_game(shell.runtime, message) ||
        state.mode != Mode::StageTransition ||
        !state.pending_stage_transition.has_value() ||
        state.game_over ||
        state.pause) {
        std::cerr << "Gubsy shell smoke failed: peer Play discarded pending transition: "
                  << message << '\n';
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_shell::Shutdown(shell);
    return true;
}

bool CheckReadyPeerAutoLeavesStaleTransition() {
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: InitHeadless for stale ready peer failed\n";
        return false;
    }

    state.SetMode(Mode::StageTransition);
    state.net_session.role = network::NetRole::Peer;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.local_player_id = 2;
    state.net_session.quest_id = "classic";
    state.net_session.quest_stage_id = "classic_mines_1";
    state.net_transport =
        std::make_unique<network::NetTransportRuntime>(network::NetTransportRuntime::New());
    state.net_transport->join_request_pending = false;

    EngineState& engine = gubsy_runtime_engine(shell.runtime);
    engine.lobby.online = true;
    engine.lobby.is_host = false;
    engine.lobby.room_code = "ROOM1";
    engine.lobby.contract.session_phase = "in_game";
    engine.lobby.contract.realtime_endpoint = "127.0.0.1:35355";

    gubsy_shell::UpdateRuntime(shell, 0.016F);
    if (state.mode != Mode::Playing ||
        state.pending_stage_transition.has_value() ||
        state.game_over ||
        state.pause ||
        gubsy_shell::InGameMenuOpen(shell)) {
        std::cerr << "Gubsy shell smoke failed: ready peer did not auto-leave stale transition\n";
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
    if (!gubsy_host_lobby_direct(shell.runtime, 0, status)) {
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
        state.net_session.role != network::NetRole::Offline ||
        gubsy_get_lobby_state(shell.runtime).online || state.pause) {
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
    if (!PumpJoinUntilConfirmed(host_shell, host_state, guest_shell, guest_state, [&]() {
            const GubsyLobbyState& guest_lobby = gubsy_get_lobby_state(guest_shell.runtime);
            return matchmaking.join_called && guest_lobby.online &&
                   guest_lobby.room_code == matchmaking.room.room_code;
        })) {
        const GubsyLobbyState& guest_lobby = gubsy_get_lobby_state(guest_shell.runtime);
        std::cerr << "Gubsy shell smoke failed: public join was not confirmed: "
                  << guest_lobby.status_message << '\n';
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
    const EngineState& host_engine = gubsy_runtime_engine(host_shell.runtime);
    const MenuWidget* host_public = GubsyWidgetBySlot(host_engine, SettingsObjectID::CARD4);
    if (host_public == nullptr || host_public->label == nullptr ||
        std::string(host_public->label) != "Host Public") {
        std::cerr << "Gubsy shell smoke failed: host setup missing Host Public bottom action\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    const SDL_FRect max_players_rect = GubsyWidgetRectBySlot(host_engine, SettingsObjectID::CARD2);
    const SDL_FRect host_public_rect = GubsyWidgetRectBySlot(host_engine, SettingsObjectID::CARD4);
    if (!RectHasArea(host_public_rect) || !RectHasArea(max_players_rect) ||
        host_public_rect.y <= max_players_rect.y) {
        std::cerr << "Gubsy shell smoke failed: Host Public is not below host setup form rows\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    PressDown(host_shell, host_state);
    PressDown(host_shell, host_state);
    PressDown(host_shell, host_state);
    PressRight(host_shell, host_state);
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

    if (!gubsy_push_menu_screen(guest_shell.runtime, MenuScreenID::LOBBY_JOIN_BY_IP)) {
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

bool CheckMultiLocalPlayerJoin() {
    std::string message;
    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Gubsy shell smoke failed: multi-local host InitHeadless failed\n";
        return false;
    }
    if (!gubsy_host_lobby_direct(host_shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell smoke failed: multi-local direct host failed: " << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    const GubsyLobbyState& host_lobby = gubsy_get_lobby_state(host_shell.runtime);
    std::string direct_host;
    std::uint16_t direct_port = 0;
    if (!ParseEndpoint(host_lobby.advertised_endpoint, direct_host, direct_port)) {
        std::cerr << "Gubsy shell smoke failed: multi-local direct host endpoint invalid\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    State guest_state = State::New();
    gubsy_shell::Shell guest_shell;
    if (!gubsy_shell::InitHeadless(guest_shell, guest_state)) {
        std::cerr << "Gubsy shell smoke failed: multi-local guest InitHeadless failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (gubsy_add_lobby_local_player(guest_shell.runtime) != 1) {
        std::cerr << "Gubsy shell smoke failed: multi-local guest did not add second local player\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    const GubsyLobbyState& guest_lobby_before_join = gubsy_get_lobby_state(guest_shell.runtime);
    if (guest_lobby_before_join.local_players.size() != 2) {
        std::cerr << "Gubsy shell smoke failed: multi-local Gubsy lobby did not keep two local players\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    if (!gubsy_join_lobby_direct(guest_shell.runtime, direct_host, direct_port, message)) {
        std::cerr << "Gubsy shell smoke failed: multi-local direct join failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!PumpJoinUntilConfirmed(host_shell, host_state, guest_shell, guest_state, [&]() {
            std::size_t host_remote_players = 0;
            for (const PlayerSlot& slot : host_state.players.slots) {
                if (slot.connected && slot.connection_kind == PlayerConnectionKind::Remote)
                    ++host_remote_players;
            }
            std::size_t guest_local_players = 0;
            for (const PlayerSlot& slot : guest_state.players.slots) {
                if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local)
                    ++guest_local_players;
            }
            return host_remote_players == 2 && guest_local_players == 2;
        })) {
        std::size_t host_remote_players = 0;
        std::size_t guest_local_players = 0;
        for (const PlayerSlot& slot : host_state.players.slots) {
            if (slot.connected && slot.connection_kind == PlayerConnectionKind::Remote)
                ++host_remote_players;
        }
        for (const PlayerSlot& slot : guest_state.players.slots) {
            if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local)
                ++guest_local_players;
        }
        std::cerr << "Gubsy shell smoke failed: multi-local join did not produce two remote/local players: host_remote="
                  << host_remote_players << " guest_local=" << guest_local_players << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    Graphics graphics;
    PumpTitleNetwork(host_shell, host_state, graphics);
    const GubsyLobbyState& synced_host_lobby = gubsy_get_lobby_state(host_shell.runtime);
    if (synced_host_lobby.game_members.size() != 2) {
        std::cerr << "Gubsy shell smoke failed: multi-local host lobby did not expose two remote members: "
                  << synced_host_lobby.game_members.size() << '\n';
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

bool CheckNetworkHostStartSynchronizesStage() {
    std::string message;
    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Gubsy shell smoke failed: network start host InitHeadless failed\n";
        return false;
    }
    if (!gubsy_host_lobby_direct(host_shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell smoke failed: network start direct host failed: "
                  << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    const GubsyLobbyState& host_lobby = gubsy_get_lobby_state(host_shell.runtime);
    std::string direct_host;
    std::uint16_t direct_port = 0;
    if (!ParseEndpoint(host_lobby.advertised_endpoint, direct_host, direct_port)) {
        std::cerr << "Gubsy shell smoke failed: network start host endpoint invalid\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    State guest_state = State::New();
    gubsy_shell::Shell guest_shell;
    if (!gubsy_shell::InitHeadless(guest_shell, guest_state)) {
        std::cerr << "Gubsy shell smoke failed: network start guest InitHeadless failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!gubsy_join_lobby_direct(guest_shell.runtime, direct_host, direct_port, message)) {
        std::cerr << "Gubsy shell smoke failed: network start direct join failed: "
                  << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!PumpJoinUntilConfirmed(host_shell, host_state, guest_shell, guest_state, [&]() {
            return host_state.net_session.role == network::NetRole::Host &&
                   guest_state.net_session.role == network::NetRole::Peer &&
                   host_state.net_transport != nullptr &&
                   !host_state.net_transport->remotes.empty() &&
                   !guest_state.net_transport->join_request_pending;
        })) {
        std::cerr << "Gubsy shell smoke failed: network start join was not confirmed\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    if (!gubsy_start_lobby_game(host_shell.runtime, message)) {
        std::cerr << "Gubsy shell smoke failed: network host start failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    Graphics host_graphics;
    Graphics guest_graphics;
    Audio host_audio;
    Audio guest_audio;
    bool matched_started_stage = false;
    for (int i = 0; i < 240; ++i) {
        StepSingleTick(host_state, host_audio, host_graphics);
        gubsy_shell::UpdateRuntime(host_shell, 0.016F);
        StepSingleTick(guest_state, guest_audio, guest_graphics);
        gubsy_shell::UpdateRuntime(guest_shell, 0.016F);
        if (host_state.net_session.stage_instance_id == guest_state.net_session.stage_instance_id &&
            host_state.net_session.stage_seed == guest_state.net_session.stage_seed &&
            host_state.stage.generation_seed == guest_state.stage.generation_seed &&
            host_state.net_session.lockstep_next_frame_to_step > 0 &&
            guest_state.net_session.lockstep_next_frame_to_step > 0) {
            matched_started_stage = true;
            break;
        }
    }
    if (!matched_started_stage) {
        std::cerr << "Gubsy shell smoke failed: network host start did not synchronize stage:"
                  << " host_instance=" << host_state.net_session.stage_instance_id
                  << " guest_instance=" << guest_state.net_session.stage_instance_id
                  << " host_seed=" << host_state.net_session.stage_seed
                  << " guest_seed=" << guest_state.net_session.stage_seed
                  << " host_frame=" << host_state.net_session.lockstep_next_frame_to_step
                  << " guest_frame=" << guest_state.net_session.lockstep_next_frame_to_step
                  << '\n';
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

bool CheckPublicMultiLocalPlayerJoin() {
    SmokeMatchmaking matchmaking;
    std::string message;
    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Gubsy shell smoke failed: public multi-local host InitHeadless failed\n";
        return false;
    }
    gubsy_set_lobby_matchmaking_backend(host_shell.runtime, &matchmaking);
    if (!gubsy_host_lobby_room(host_shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell smoke failed: public multi-local host failed: " << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    State guest_state = State::New();
    gubsy_shell::Shell guest_shell;
    if (!gubsy_shell::InitHeadless(guest_shell, guest_state)) {
        std::cerr << "Gubsy shell smoke failed: public multi-local guest InitHeadless failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    gubsy_set_lobby_matchmaking_backend(guest_shell.runtime, &matchmaking);
    if (gubsy_add_lobby_local_player(guest_shell.runtime) != 1) {
        std::cerr << "Gubsy shell smoke failed: public multi-local guest did not add second local player\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!gubsy_join_lobby_room_code(guest_shell.runtime, matchmaking.room.room_code, message)) {
        std::cerr << "Gubsy shell smoke failed: public multi-local join failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!PumpJoinUntilConfirmed(host_shell, host_state, guest_shell, guest_state, [&]() {
            std::size_t host_remote_players = 0;
            for (const PlayerSlot& slot : host_state.players.slots) {
                if (slot.connected && slot.connection_kind == PlayerConnectionKind::Remote)
                    ++host_remote_players;
            }
            return host_remote_players == 2 && guest_state.net_session.role == network::NetRole::Peer;
        })) {
        std::cerr << "Gubsy shell smoke failed: public multi-local join did not create two host remote players\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    Graphics graphics;
    PumpTitleNetwork(host_shell, host_state, graphics);
    const GubsyLobbyState& host_lobby = gubsy_get_lobby_state(host_shell.runtime);
    if (host_lobby.game_members.size() != 2 ||
        host_lobby.game_members[0].member_id.rfind("direct:player:", 0) != 0 ||
        host_lobby.game_members[1].member_id.rfind("direct:player:", 0) != 0) {
        std::cerr << "Gubsy shell smoke failed: public multi-local host lobby did not expose two remote players: "
                  << host_lobby.game_members.size() << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    (void)gubsy_leave_lobby_room(guest_shell.runtime, message);
    for (int i = 0; i < 60; ++i) {
        PumpTitleNetwork(host_shell, host_state, graphics);
        if (gubsy_get_lobby_state(host_shell.runtime).game_members.empty())
            break;
    }
    if (!gubsy_get_lobby_state(host_shell.runtime).game_members.empty()) {
        std::cerr << "Gubsy shell smoke failed: public multi-local leave did not remove all remote players\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    if (!gubsy_join_lobby_room_code(guest_shell.runtime, matchmaking.room.room_code, message)) {
        std::cerr << "Gubsy shell smoke failed: public multi-local rejoin failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!PumpJoinUntilConfirmed(host_shell, host_state, guest_shell, guest_state, [&]() {
            std::size_t host_remote_players = 0;
            for (const PlayerSlot& slot : host_state.players.slots) {
                if (slot.connected && slot.connection_kind == PlayerConnectionKind::Remote)
                    ++host_remote_players;
            }
            return host_remote_players == 2 && guest_state.net_session.role == network::NetRole::Peer;
        })) {
        std::cerr << "Gubsy shell smoke failed: public multi-local rejoin did not recreate two host remote players\n";
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

bool CheckRealRoomdHostJoin() {
    const char* server_url_env = std::getenv("GUB_ROOM_SERVER_URL");
    if (server_url_env == nullptr || *server_url_env == '\0') {
        std::cerr << "Gubsy shell real-roomd smoke skipped: GUB_ROOM_SERVER_URL is unset\n";
        return true;
    }

    RoomServerMatchmaking matchmaking;
    std::string message;

    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: host InitHeadless failed\n";
        return false;
    }
    EngineState& host_engine = gubsy_runtime_engine(host_shell.runtime);
    host_engine.lobby.room_server_url = server_url_env;
    host_engine.lobby.visibility = GubsyLobbyVisibility::Public;
    if (!gubsy_host_lobby_room(host_shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: host room failed: " << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    const GubsyLobbyState& host_lobby = gubsy_get_lobby_state(host_shell.runtime);
    if (!host_lobby.online || !host_lobby.is_host || host_lobby.room_code.empty() ||
        host_state.net_session.role != network::NetRole::Host) {
        std::cerr << "Gubsy shell real-roomd smoke failed: public host state invalid\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    std::vector<MatchmakingRoom> rooms;
    std::string err;
    if (!matchmaking.list_rooms(server_url_env, rooms, err)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: list rooms failed: " << err << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    const auto host_room_it = std::find_if(rooms.begin(), rooms.end(), [&](const MatchmakingRoom& room) {
        return room.room_code == host_lobby.room_code;
    });
    if (host_room_it == rooms.end() || host_room_it->privacy <= 0 ||
        host_room_it->contract.realtime_endpoint != host_lobby.advertised_endpoint) {
        std::cerr << "Gubsy shell real-roomd smoke failed: hosted room was not listed correctly\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    State guest_state = State::New();
    gubsy_shell::Shell guest_shell;
    if (!gubsy_shell::InitHeadless(guest_shell, guest_state)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: guest InitHeadless failed\n";
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    gubsy_runtime_engine(guest_shell.runtime).lobby.room_server_url = server_url_env;
    if (!gubsy_join_lobby_room_code(guest_shell.runtime, host_lobby.room_code, message)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: join room failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if (!PumpJoinUntilConfirmed(host_shell, host_state, guest_shell, guest_state, [&]() {
            const GubsyLobbyState& guest_lobby = gubsy_get_lobby_state(guest_shell.runtime);
            return guest_lobby.online && !guest_lobby.is_host &&
                   guest_lobby.room_code == host_lobby.room_code &&
                   guest_state.net_session.role == network::NetRole::Peer;
        })) {
        const GubsyLobbyState& guest_lobby = gubsy_get_lobby_state(guest_shell.runtime);
        std::cerr << "Gubsy shell real-roomd smoke failed: public join was not confirmed: "
                  << guest_lobby.status_message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    if (!gubsy_push_menu_screen(guest_shell.runtime, MenuScreenID::SHELL_LOBBY)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: guest shell lobby screen missing\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    StepMenu(guest_shell, guest_state);
    const EngineState& waiting_engine = gubsy_runtime_engine(guest_shell.runtime);
    const MenuWidget* waiting_action = GubsyWidgetBySlot(waiting_engine, SettingsObjectID::ACTION);
    if (waiting_action == nullptr || waiting_action->label == nullptr ||
        std::string(waiting_action->label) != "Waiting For Host") {
        std::cerr << "Gubsy shell real-roomd smoke failed: joined guest did not wait for host\n";
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    if (!gubsy_start_lobby_game(host_shell.runtime, message)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: host start failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    bool guest_waited_for_host_state = false;
    for (int i = 0; i < 120; ++i) {
        gubsy_shell::UpdateRuntime(host_shell, 0.016F);
        StepMenu(guest_shell, guest_state);
        const EngineState& guest_engine = gubsy_runtime_engine(guest_shell.runtime);
        const MenuWidget* guest_action = GubsyWidgetBySlot(guest_engine, SettingsObjectID::ACTION);
        if (guest_shell.joined_room_host_in_game &&
            ((guest_action != nullptr && guest_action->label != nullptr &&
              std::string(guest_action->label) == "Waiting For Host") ||
             guest_state.mode == Mode::StageTransition)) {
            guest_waited_for_host_state = true;
            break;
        }
    }
    if (!guest_waited_for_host_state) {
        const GubsyLobbyState& guest_lobby = gubsy_get_lobby_state(guest_shell.runtime);
        std::cerr << "Gubsy shell real-roomd smoke failed: guest did not wait for host state: "
                  << guest_lobby.contract.session_phase << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    guest_state.net_session.input_lockstep_enabled = false;
    bool guest_saw_play = false;
    bool guest_entered_from_sync = false;
    for (int i = 0; i < 30; ++i) {
        StepMenu(guest_shell, guest_state);
        if (guest_state.mode == Mode::Playing ||
            (guest_state.mode == Mode::StageTransition &&
             !gubsy_shell::InGameMenuOpen(guest_shell))) {
            guest_entered_from_sync = true;
            break;
        }
        const EngineState& guest_engine = gubsy_runtime_engine(guest_shell.runtime);
        const MenuWidget* play_action = GubsyWidgetBySlot(guest_engine, SettingsObjectID::ACTION);
        if (play_action != nullptr && play_action->label != nullptr &&
            std::string(play_action->label) == "Play") {
            guest_saw_play = true;
            break;
        }
    }
    if (!guest_saw_play && !guest_entered_from_sync) {
        const GubsyLobbyState& guest_lobby = gubsy_get_lobby_state(guest_shell.runtime);
        std::cerr << "Gubsy shell real-roomd smoke failed: guest never saw Play after host start: "
                  << guest_lobby.contract.session_phase << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    if (guest_saw_play && !gubsy_start_lobby_game(guest_shell.runtime, message)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: guest Play did not enter gameplay: "
                  << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        gubsy_shell::Shutdown(host_shell);
        return false;
    }
    if ((guest_state.mode != Mode::Playing && guest_state.mode != Mode::StageTransition) ||
        gubsy_shell::InGameMenuOpen(guest_shell)) {
        std::cerr << "Gubsy shell real-roomd smoke failed: guest did not enter hosted flow: "
                  << message << '\n';
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

bool CheckDirectRemoteMemberSync() {
    std::string message;
    State state = State::New();
    gubsy_shell::Shell shell;
    if (!gubsy_shell::InitHeadless(shell, state)) {
        std::cerr << "Gubsy shell smoke failed: direct member sync InitHeadless failed\n";
        return false;
    }
    if (!gubsy_host_lobby_direct(shell.runtime, 0, message)) {
        std::cerr << "Gubsy shell smoke failed: direct member sync host failed: " << message
                  << '\n';
        gubsy_shell::Shutdown(shell);
        return false;
    }

    state.players.EnsureRemotePlayer(2, "Remote Friend");
    network::NetPeerState peer;
    peer.player_id = 2;
    peer.display_name = "Remote Friend";
    peer.endpoint_address = "192.0.2.55";
    peer.endpoint_port = 45454;
    peer.connected = true;
    state.net_session.peers.push_back(peer);
    state.net_transport->remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = {.address = "192.0.2.55", .port = 45454},
    });

    StepMenu(shell, state);
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(shell.runtime);
    EngineState& engine = gubsy_runtime_engine(shell.runtime);
    if (lobby.game_members.size() != 1 || lobby.game_members.front().display_name != "Remote Friend" ||
        lobby.game_members.front().member_id != "direct:player:2" ||
        lobby.game_members.front().client_label != "192.0.2.55:45454") {
        std::cerr << "Gubsy shell smoke failed: direct remote member was not synced into Gubsy\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!GubsyAlertContains(engine, "Remote Friend joined")) {
        std::cerr << "Gubsy shell smoke failed: direct remote join alert missing\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!GubsyAlertContains(engine, "Remote Friend joined from client 192.0.2.55:45454")) {
        std::cerr << "Gubsy shell smoke failed: direct remote join client alert missing\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    gubsy_clear_menu_stack(shell.runtime);
    if (!gubsy_push_menu_screen(shell.runtime, MenuScreenID::SHELL_LOBBY)) {
        std::cerr << "Gubsy shell smoke failed: shell lobby missing for direct member sync\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    StepMenu(shell, state);
    const MenuWidget* status = GubsyWidgetBySlot(engine, SettingsObjectID::STATUS_RIGHT);
    const MenuWidget* players = GubsyWidgetBySlot(engine, SettingsObjectID::CARD0);
    if (status == nullptr || status->secondary == nullptr ||
        std::string(status->secondary).find("1 remote player") == std::string::npos ||
        players == nullptr || players->secondary == nullptr ||
        std::string(players->secondary).find("1 remote player") == std::string::npos) {
        std::cerr << "Gubsy shell smoke failed: direct remote count missing from lobby UI\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    if (!gubsy_lobby_kick_direct_member(engine, lobby.game_members.front(), message)) {
        std::cerr << "Gubsy shell smoke failed: direct remote kick failed: " << message << '\n';
        gubsy_shell::Shutdown(shell);
        return false;
    }
    StepMenu(shell, state);
    if (state.players.Find(2) != nullptr || !state.net_transport->remotes.empty()) {
        std::cerr << "Gubsy shell smoke failed: direct remote kick did not remove network peer\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!gubsy_get_lobby_state(shell.runtime).game_members.empty()) {
        std::cerr << "Gubsy shell smoke failed: direct remote kick did not clear Gubsy member\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!GubsyAlertContains(engine, "Kicked direct player")) {
        std::cerr << "Gubsy shell smoke failed: direct remote kick alert missing\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    state.players.EnsureRemotePlayer(2, "Remote Friend");
    state.net_session.peers.clear();
    state.net_session.peers.push_back(peer);
    state.net_transport->remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = {.address = "192.0.2.55", .port = 45454},
    });
    StepMenu(shell, state);

    state.players.Remove(2);
    state.net_session.peers.clear();
    state.net_transport->remotes.clear();
    StepMenu(shell, state);
    if (!gubsy_get_lobby_state(shell.runtime).game_members.empty()) {
        std::cerr << "Gubsy shell smoke failed: direct remote member was not removed from Gubsy\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!GubsyAlertContains(engine, "Remote Friend left")) {
        std::cerr << "Gubsy shell smoke failed: direct remote leave alert missing\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }
    if (!GubsyAlertContains(engine, "Remote Friend left from client 192.0.2.55:45454")) {
        std::cerr << "Gubsy shell smoke failed: direct remote leave client alert missing\n";
        gubsy_shell::Shutdown(shell);
        return false;
    }

    (void)gubsy_leave_lobby_room(shell.runtime, message);
    gubsy_shell::Shutdown(shell);
    return true;
}

bool HostHasRemotePeer(const State& state) {
    return state.net_transport != nullptr && !state.net_transport->remotes.empty();
}

bool LockstepHealthyInGameplay(const State& state) {
    return state.net_session.input_lockstep_enabled &&
           state.net_session.lockstep_next_frame_to_step > 60 &&
           state.net_session.lockstep_has_confirmed_hash &&
           state.net_session.lockstep_hash_mismatch_count == 0 &&
           state.net_session.lockstep_last_desync_recovery_mode !=
               network::LockstepDesyncRecoveryMode::FatalDesync &&
           !state.net_session.join_barrier_active;
}

bool CheckRealnetLanHost(const char* server_url, int max_frames) {
    if (server_url == nullptr || *server_url == '\0') {
        std::cerr << "Realnet LAN host smoke failed: missing room server URL\n";
        return false;
    }

    std::string message;
    State host_state = State::New();
    gubsy_shell::Shell host_shell;
    if (!gubsy_shell::InitHeadless(host_shell, host_state)) {
        std::cerr << "Realnet LAN host smoke failed: InitHeadless failed\n";
        return false;
    }

    EngineState& host_engine = gubsy_runtime_engine(host_shell.runtime);
    host_engine.lobby.room_server_url = server_url;
    host_engine.lobby.visibility = GubsyLobbyVisibility::Public;
    if (!gubsy_host_lobby_room(host_shell.runtime, 0, message)) {
        std::cerr << "Realnet LAN host smoke failed: host room failed: " << message << '\n';
        gubsy_shell::Shutdown(host_shell);
        return false;
    }

    const GubsyLobbyState& host_lobby = gubsy_get_lobby_state(host_shell.runtime);
    std::cout << "REALNET_LAN_HOST_READY room_code=" << host_lobby.room_code
              << " endpoint=" << host_lobby.advertised_endpoint << '\n';
    std::cout.flush();

    Graphics graphics;
    Audio audio;
    bool started = false;
    bool healthy = false;
    for (int frame = 0; frame < max_frames; ++frame) {
        StepHeadlessShell(host_shell, host_state, graphics, audio);
        if (!started && HostHasRemotePeer(host_state)) {
            if (!gubsy_start_lobby_game(host_shell.runtime, message)) {
                std::cerr << "Realnet LAN host smoke failed: start failed: " << message << '\n';
                gubsy_shell::Shutdown(host_shell);
                return false;
            }
            EngineState& engine = gubsy_runtime_engine(host_shell.runtime);
            engine.lobby.contract.session_phase = "in_game";
            gubsy_lobby_force_online_tick(engine);
            started = true;
            std::cout << "REALNET_LAN_HOST_STARTED\n";
            std::cout.flush();
        }
        if (!healthy && started && LockstepHealthyInGameplay(host_state)) {
            healthy = true;
            std::cout << "REALNET_LAN_HOST_OK frame="
                      << host_state.net_session.lockstep_next_frame_to_step << '\n';
            std::cout.flush();
        }
        SDL_Delay(16);
    }

    if (healthy) {
        (void)gubsy_leave_lobby_room(host_shell.runtime, message);
        gubsy_shell::Shutdown(host_shell);
        return true;
    }

    std::cerr << "Realnet LAN host smoke failed: timed out"
              << " started=" << (started ? "true" : "false")
              << " remotes=" << (host_state.net_transport ? host_state.net_transport->remotes.size() : 0)
              << " frame=" << host_state.net_session.lockstep_next_frame_to_step
              << " barrier=" << (host_state.net_session.join_barrier_active ? "true" : "false")
              << " mismatches=" << host_state.net_session.lockstep_hash_mismatch_count << '\n';
    (void)gubsy_leave_lobby_room(host_shell.runtime, message);
    gubsy_shell::Shutdown(host_shell);
    return false;
}

std::optional<MatchmakingRoom> FindJoinableRoom(const char* server_url,
                                                const char* room_code,
                                                std::string& err) {
    RoomServerMatchmaking matchmaking;
    if (room_code != nullptr && *room_code != '\0') {
        MatchmakingRoom room;
        if (matchmaking.fetch_room(server_url, room_code, room, err))
            return room;
        return std::nullopt;
    }

    std::vector<MatchmakingRoom> rooms;
    if (!matchmaking.list_rooms(server_url, rooms, err))
        return std::nullopt;
    auto it = std::find_if(rooms.begin(), rooms.end(), [](const MatchmakingRoom& room) {
        return room.privacy > 0 && !session_contract_is_in_game(room.contract);
    });
    if (it == rooms.end()) {
        err = "no lobby-phase public room found";
        return std::nullopt;
    }
    return *it;
}

bool CheckRealnetLanClient(const char* server_url, const char* room_code, int max_frames) {
    if (server_url == nullptr || *server_url == '\0') {
        std::cerr << "Realnet LAN client smoke failed: missing room server URL\n";
        return false;
    }

    std::string err;
    std::optional<MatchmakingRoom> room;
    for (int attempt = 0; attempt < 120 && !room.has_value(); ++attempt) {
        room = FindJoinableRoom(server_url, room_code, err);
        if (!room.has_value())
            SDL_Delay(100);
    }
    if (!room.has_value()) {
        std::cerr << "Realnet LAN client smoke failed: room lookup failed: " << err << '\n';
        return false;
    }

    std::string message;
    State guest_state = State::New();
    gubsy_shell::Shell guest_shell;
    if (!gubsy_shell::InitHeadless(guest_shell, guest_state)) {
        std::cerr << "Realnet LAN client smoke failed: InitHeadless failed\n";
        return false;
    }
    gubsy_runtime_engine(guest_shell.runtime).lobby.room_server_url = server_url;
    if (!gubsy_join_lobby_room_code(guest_shell.runtime, room->room_code, message)) {
        std::cerr << "Realnet LAN client smoke failed: join failed: " << message << '\n';
        gubsy_shell::Shutdown(guest_shell);
        return false;
    }
    std::cout << "REALNET_LAN_CLIENT_JOINING room_code=" << room->room_code << '\n';
    std::cout.flush();

    Graphics graphics;
    Audio audio;
    bool requested_play = false;
    bool moved_right = false;
    std::optional<float> start_x;
    std::optional<float> last_x;
    PlayerId last_local_player_id = kInvalidPlayerId;
    bool last_local_input_right = false;
    bool last_buffer_input_right = false;
    bool last_buffer_input_canonical = false;
    bool last_buffer_input_predicted = false;
    for (int frame = 0; frame < max_frames; ++frame) {
        EngineState& engine = gubsy_runtime_engine(guest_shell.runtime);
        if (frame % 15 == 0)
            gubsy_lobby_force_online_tick(engine);
        StepHeadlessShell(guest_shell, guest_state, graphics, audio, requested_play);

        if (!requested_play && guest_shell.joined_room_host_in_game) {
            if (!gubsy_start_lobby_game(guest_shell.runtime, message)) {
                std::cerr << "Realnet LAN client smoke failed: Play failed: " << message << '\n';
                gubsy_shell::Shutdown(guest_shell);
                return false;
            }
            requested_play = true;
            std::cout << "REALNET_LAN_CLIENT_PLAY\n";
            std::cout.flush();
        }

        const PlayerSlot* local_slot = guest_state.players.FindPrimaryLocal();
        const Ent* local_ent = nullptr;
        if (local_slot != nullptr && local_slot->ent_vid.has_value()) {
            local_ent = guest_state.ents.GetEnt(*local_slot->ent_vid);
        }
        if (requested_play && local_ent != nullptr && guest_state.mode == Mode::Playing) {
            last_local_player_id = local_slot != nullptr ? local_slot->player_id : kInvalidPlayerId;
            last_x = local_ent->pos.x;
            last_local_input_right = guest_state.playing_input_snapshot.right;
            if (local_slot != nullptr && local_slot->player_id != kInvalidPlayerId &&
                guest_state.net_session.lockstep_next_local_input_frame > 0) {
                const network::LockstepFrame latest_frame =
                    guest_state.net_session.lockstep_next_local_input_frame - 1;
                if (const network::LockstepInputRecord* record =
                        guest_state.net_session.lockstep_input_buffer.FindRecord(
                            local_slot->player_id, latest_frame)) {
                    last_buffer_input_right = record->input.right;
                    last_buffer_input_canonical = record->canonical;
                    last_buffer_input_predicted = record->predicted;
                }
            }
            if (!start_x.has_value())
                start_x = local_ent->pos.x;
            if (local_ent->pos.x > *start_x + 0.25F)
                moved_right = true;
        }

        if (moved_right && LockstepHealthyInGameplay(guest_state)) {
            std::cout << "REALNET_LAN_CLIENT_OK frame="
                      << guest_state.net_session.lockstep_next_frame_to_step << '\n';
            (void)gubsy_leave_lobby_room(guest_shell.runtime, message);
            gubsy_shell::Shutdown(guest_shell);
            return true;
        }
        SDL_Delay(16);
    }

    std::cerr << "Realnet LAN client smoke failed: timed out"
              << " requested_play=" << (requested_play ? "true" : "false")
              << " moved_right=" << (moved_right ? "true" : "false")
              << " role=" << static_cast<int>(guest_state.net_session.role)
              << " mode=" << static_cast<int>(guest_state.mode)
              << " frame=" << guest_state.net_session.lockstep_next_frame_to_step
              << " local_input_frame=" << guest_state.net_session.lockstep_next_local_input_frame
              << " barrier=" << (guest_state.net_session.join_barrier_active ? "true" : "false")
              << " mismatches=" << guest_state.net_session.lockstep_hash_mismatch_count
              << " local_player=" << last_local_player_id
              << " start_x=" << (start_x.has_value() ? std::to_string(*start_x) : "none")
              << " last_x=" << (last_x.has_value() ? std::to_string(*last_x) : "none")
              << " snapshot_right=" << (last_local_input_right ? "true" : "false")
              << " buffer_right=" << (last_buffer_input_right ? "true" : "false")
              << " buffer_canonical=" << (last_buffer_input_canonical ? "true" : "false")
              << " buffer_predicted=" << (last_buffer_input_predicted ? "true" : "false")
              << " arbitrated_missing="
              << guest_state.net_session.lockstep_arbitrated_missing_input_count
              << " online=" << (gubsy_get_lobby_state(guest_shell.runtime).online ? "true" : "false")
              << " is_host=" << (gubsy_get_lobby_state(guest_shell.runtime).is_host ? "true" : "false")
              << " phase=" << gubsy_get_lobby_state(guest_shell.runtime).contract.session_phase
              << " status=\"" << gubsy_get_lobby_state(guest_shell.runtime).status_message << "\""
              << '\n';
    (void)gubsy_leave_lobby_room(guest_shell.runtime, message);
    gubsy_shell::Shutdown(guest_shell);
    return false;
}

} // namespace

bool CheckGubsyShellSmoke() {
    try {
        return CheckOfflineStart() && CheckInGameMenuShell() && CheckInGameRestartCommand() &&
               CheckNetworkRestartCommandDoesNotDesync() && CheckPeerInputMapping() &&
               CheckPeerGameplayInputAfterStart() && CheckPeerPlayLeavesStaleTransition() &&
               CheckReadyPeerAutoLeavesStaleTransition() && CheckInGameQuitCommand() && CheckHostJoin() &&
               CheckDirectHostJoinViaMenu() && CheckHostStartKeepsLocalPlayers() &&
               CheckMultiLocalPlayerJoin() && CheckNetworkHostStartSynchronizesStage() &&
               CheckPublicMultiLocalPlayerJoin() && CheckDirectRemoteMemberSync();
    } catch (const std::exception& e) {
        std::cerr << "Gubsy shell smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckGubsyShellRealRoomdSmoke() {
    try {
        return CheckRealRoomdHostJoin();
    } catch (const std::exception& e) {
        std::cerr << "Gubsy shell real-roomd smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckGubsyShellRealnetLanHost(const char* server_url, int max_frames) {
    try {
        return CheckRealnetLanHost(server_url, max_frames);
    } catch (const std::exception& e) {
        std::cerr << "Realnet LAN host smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckGubsyShellRealnetLanClient(const char* server_url, const char* room_code,
                                     int max_frames) {
    try {
        return CheckRealnetLanClient(server_url, room_code, max_frames);
    } catch (const std::exception& e) {
        std::cerr << "Realnet LAN client smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks

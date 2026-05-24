#include "gubsy_shell.hpp"
#include "input_bind_schema.hpp"

#include <algorithm>

namespace splonks::gubsy_shell {
namespace {

bool ActionDown(Shell& shell, int player_index, SplonksGubsyAction action) {
    return gubsy_lobby_player_action_down(shell.runtime, player_index, static_cast<int>(action));
}

InputFrame BuildLobbyInputFrame(Shell& shell, int player_index) {
    InputFrame frame = InputFrame::New();
    frame.left = ActionDown(shell, player_index, kGubsyActionMoveLeft);
    frame.right = ActionDown(shell, player_index, kGubsyActionMoveRight);
    frame.up = ActionDown(shell, player_index, kGubsyActionMoveUp);
    frame.down = ActionDown(shell, player_index, kGubsyActionMoveDown);
    frame.jump = ActionDown(shell, player_index, kGubsyActionConfirmJump);
    frame.attack = ActionDown(shell, player_index, kGubsyActionBackAttack);
    frame.bomb = ActionDown(shell, player_index, kGubsyActionPagePreviousBomb);
    frame.rope = ActionDown(shell, player_index, kGubsyActionPageNextRope);
    frame.use_button = ActionDown(shell, player_index, kGubsyActionUse);
    frame.buy_button = frame.use_button;
    return frame;
}

} // namespace

void ApplyLobbyGameplayInput(Shell& shell) {
    if (shell.state == nullptr)
        return;

    const GubsyLobbyState& lobby = gubsy_get_lobby_state(shell.runtime);
    const std::size_t player_count = std::max<std::size_t>(lobby.local_players.size(), 1);
    shell.state->external_local_input_frames.resize(player_count);
    for (std::size_t i = 0; i < player_count; ++i) {
        shell.state->external_local_input_frames[i] =
            BuildLobbyInputFrame(shell, static_cast<int>(i));
    }
    shell.state->use_external_local_input_frames = true;
}

} // namespace splonks::gubsy_shell

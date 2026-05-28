#include "gubsy_shell.hpp"
#include "input_bind_schema.hpp"

#include <algorithm>
#include <vector>

namespace splonks::gubsy_shell {
namespace {

bool ActionDown(Shell& shell, int player_index, SplonksGubsyAction action) {
    return gubsy_lobby_player_action_down(shell.runtime, player_index, static_cast<int>(action));
}

bool AxisDown(Shell& shell, int player_index, SplonksGubsyAxis1D axis) {
    return gubsy_lobby_player_axis_1d_down(shell.runtime, player_index, static_cast<int>(axis),
                                           0.35F);
}

InputFrame BuildLobbyInputFrame(Shell& shell, int player_index) {
    InputFrame frame = InputFrame::New();
    frame.left = ActionDown(shell, player_index, kGubsyActionMoveLeft);
    frame.right = ActionDown(shell, player_index, kGubsyActionMoveRight);
    frame.up = ActionDown(shell, player_index, kGubsyActionMoveUp);
    frame.down = ActionDown(shell, player_index, kGubsyActionMoveDown);
    frame.jump = ActionDown(shell, player_index, kGubsyActionConfirmJump);
    frame.run = ActionDown(shell, player_index, kGubsyActionRun) ||
                AxisDown(shell, player_index, kGubsyAxisRun);
    frame.use_button = ActionDown(shell, player_index, kGubsyActionUseBack) ||
                       AxisDown(shell, player_index, kGubsyAxisUseBack);
    frame.equip_button = ActionDown(shell, player_index, kGubsyActionEquip);
    frame.pick_up_drop = ActionDown(shell, player_index, kGubsyActionPickUpDrop);
    frame.stop = ActionDown(shell, player_index, kGubsyActionStopNextStage);
    frame.bomb = ActionDown(shell, player_index, kGubsyActionBombGrenade);
    frame.rope = ActionDown(shell, player_index, kGubsyActionRope);
    frame.attack = ActionDown(shell, player_index, kGubsyActionUse) ||
                   ActionDown(shell, player_index, kGubsyActionAttack);
    frame.buy_button = ActionDown(shell, player_index, kGubsyActionBuy);
    frame.emote_up = ActionDown(shell, player_index, kGubsyActionEmoteUp);
    frame.emote_down = ActionDown(shell, player_index, kGubsyActionEmoteDown);
    return frame;
}

std::vector<PlayerId> LocalPlayerIdsForLobbyInput(const State& state,
                                                  std::size_t lobby_player_count) {
    std::vector<PlayerId> player_ids;
    player_ids.reserve(state.players.slots.size());
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local) {
            continue;
        }
        player_ids.push_back(slot.player_id);
    }

    std::stable_sort(player_ids.begin(), player_ids.end(), [&](PlayerId a, PlayerId b) {
        const PlayerSlot* const a_slot = state.players.Find(a);
        const PlayerSlot* const b_slot = state.players.Find(b);
        const bool a_primary = a_slot != nullptr && a_slot->primary_local;
        const bool b_primary = b_slot != nullptr && b_slot->primary_local;
        if (a_primary != b_primary) {
            return a_primary;
        }
        return a < b;
    });

    for (PlayerId fallback_id = 1;
         player_ids.size() < lobby_player_count;
         ++fallback_id) {
        if (std::find(player_ids.begin(), player_ids.end(), fallback_id) == player_ids.end()) {
            player_ids.push_back(fallback_id);
        }
    }
    return player_ids;
}

} // namespace

void ApplyLobbyGameplayInput(Shell& shell) {
    if (shell.state == nullptr)
        return;

    const GubsyLobbyState& lobby = gubsy_get_lobby_state(shell.runtime);
    const std::size_t player_count = std::max<std::size_t>(lobby.local_players.size(), 1);
    const std::vector<PlayerId> player_ids = LocalPlayerIdsForLobbyInput(*shell.state, player_count);
    PlayerId max_player_id = 0;
    for (std::size_t i = 0; i < player_count; ++i) {
        max_player_id = std::max(max_player_id, player_ids[i]);
    }

    shell.state->external_local_input_frames.assign(
        static_cast<std::size_t>(max_player_id),
        InputFrame::New()
    );
    for (std::size_t i = 0; i < player_count; ++i) {
        const PlayerId player_id = player_ids[i];
        if (player_id < 1) {
            continue;
        }
        shell.state->external_local_input_frames[static_cast<std::size_t>(player_id - 1)] =
            BuildLobbyInputFrame(shell, static_cast<int>(i));
    }
    shell.state->use_external_local_input_frames = true;
}

} // namespace splonks::gubsy_shell

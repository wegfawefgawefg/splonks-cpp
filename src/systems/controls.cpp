#include "systems/controls.hpp"

#include "entities/player.hpp"

namespace splonks::systems::controls {

void ControlEntityAsPlayer(const VID& entity_vid, State& state) {
    if (Entity* const player = state.entity_manager.GetEntityMut(entity_vid)) {
        const PlayingInputs inputs = state.playing_inputs;
        if (player->super_state == EntitySuperState::Dead ||
            player->super_state == EntitySuperState::Stunned) {
            player->trying_to_go_left = false;
            player->trying_to_go_right = false;
            player->trying_to_jump = false;
            player->trying_to_use = false;
            player->trying_pick_up_drop = false;
            player->trying_to_go_up = false;
            player->trying_to_go_down = false;
            player->trying_to_equip = false;
            player->trying_to_bomb = false;
            player->trying_to_rope = false;
            player->trying_to_attack = false;
            player->was_horizontally_controlled_this_frame = false;
            return;
        }

        player->trying_to_go_left = inputs.left;
        player->trying_to_go_right = inputs.right;
        player->trying_to_jump = inputs.jump;
        player->trying_to_use = inputs.use_button;
        player->trying_pick_up_drop = inputs.pick_up_drop;
        player->trying_to_go_up = inputs.up;
        player->trying_to_go_down = inputs.down;
        player->trying_to_equip = inputs.equip_button;
        player->trying_to_bomb = inputs.bomb;
        player->trying_to_rope = inputs.rope;
        player->trying_to_attack = inputs.attack;
        player->was_horizontally_controlled_this_frame = inputs.left || inputs.right;
        player->no_hang = inputs.down;
        player->running = inputs.run;

        if (!(inputs.left && inputs.right)) {
            if (inputs.run) {
                if (inputs.left) {
                    player->acc.x = -entities::player::kRunAcc;
                }
                if (inputs.right) {
                    player->acc.x = entities::player::kRunAcc;
                }
            } else {
                if (inputs.left) {
                    player->acc.x = -entities::player::kMoveAcc;
                }
                if (inputs.right) {
                    player->acc.x = entities::player::kMoveAcc;
                }
            }
        }
        if (inputs.stop) {
            player->acc = Vec2::New(0.0F, 0.0F);
            player->vel = Vec2::New(0.0F, 0.0F);
        }
        if (inputs.jump) {
            player->jumping = inputs.jump || false;
        }
    }
}

} // namespace splonks::systems::controls

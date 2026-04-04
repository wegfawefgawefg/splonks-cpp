#include "systems/controls.hpp"

#include "entities/player.hpp"

namespace splonks::systems::controls {

void ControlEntityAsPlayer(const VID& entity_vid, State& state) {
    if (Entity* const player = state.entity_manager.GetEntityMut(entity_vid)) {
        const PlayingInputs inputs = state.playing_inputs;
        if (player->super_state == EntitySuperState::Dead ||
            player->super_state == EntitySuperState::Stunned) {
            return;
        }

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

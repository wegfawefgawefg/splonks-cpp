#include "systems/controls.hpp"

#include "entities/player.hpp"

namespace splonks::systems::controls {

ControlIntent GetControlIntentForEntity(const Entity& entity, const State& state) {
    if (!state.controlled_entity_vid.has_value() || entity.vid != *state.controlled_entity_vid) {
        return ControlIntent{};
    }
    if (entity.super_state == EntitySuperState::Dead ||
        entity.super_state == EntitySuperState::Crushed) {
        return ControlIntent{};
    }

    const PlayingInputs& inputs = state.playing_inputs;
    return ControlIntent{
        .left = inputs.left.down,
        .right = inputs.right.down,
        .up = inputs.up.down,
        .down = inputs.down.down,
        .jump_pressed = inputs.jump.pressed,
        .run = inputs.run.down,
        .use_held = inputs.use_button.down,
        .use_pressed = inputs.use_button.pressed,
        .equip_pressed = inputs.equip_button.pressed,
        .pick_up_drop_pressed = inputs.pick_up_drop.pressed,
        .bomb_pressed = inputs.bomb.pressed,
        .rope_pressed = inputs.rope.pressed,
        .attack_pressed = inputs.attack.pressed,
        .stop = inputs.stop.down,
        .no_hang = inputs.down.down,
    };
}

void ControlEntityAsPlayer(const VID& entity_vid, State& state) {
    if (Entity* const player = state.entity_manager.GetEntityMut(entity_vid)) {
        const ControlIntent intent = GetControlIntentForEntity(*player, state);
        if (player->super_state == EntitySuperState::Dead ||
            player->super_state == EntitySuperState::Stunned) {
            player->was_horizontally_controlled_this_frame = false;
            return;
        }

        player->was_horizontally_controlled_this_frame = intent.left || intent.right;

        if (!(intent.left && intent.right)) {
            if (intent.run) {
                if (intent.left) {
                    player->acc.x = -entities::player::kRunAcc;
                }
                if (intent.right) {
                    player->acc.x = entities::player::kRunAcc;
                }
            } else {
                if (intent.left) {
                    player->acc.x = -entities::player::kMoveAcc;
                }
                if (intent.right) {
                    player->acc.x = entities::player::kMoveAcc;
                }
            }
        }
        if (intent.stop) {
            player->acc = Vec2::New(0.0F, 0.0F);
            player->vel = Vec2::New(0.0F, 0.0F);
        }
    }
}

} // namespace splonks::systems::controls

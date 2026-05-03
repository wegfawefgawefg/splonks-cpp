#include "controls.hpp"


namespace splonks::controls {

ControlIntent GetControlIntentForEntity(const Entity& entity, const State& state) {
    const PlayingInputs* inputs = state.players.FindInputsForEntity(entity.vid);
    if (inputs == nullptr) {
        if (!state.controlled_entity_vid.has_value() || entity.vid != *state.controlled_entity_vid) {
            return ControlIntent{};
        }
        inputs = &state.playing_inputs;
    }
    if (entity.condition != EntityCondition::Normal) {
        return ControlIntent{};
    }

    return ControlIntent{
        .left = inputs->left.down,
        .right = inputs->right.down,
        .up = inputs->up.down,
        .down = inputs->down.down,
        .jump = inputs->jump.down,
        .jump_pressed = inputs->jump.pressed,
        .run = inputs->run.down,
        .use_held = inputs->attack.down,
        .use_pressed = inputs->attack.pressed,
        .use_back = inputs->use_button.down,
        .use_back_pressed = inputs->use_button.pressed,
        .equip_pressed = inputs->equip_button.pressed,
        .pick_up_drop_pressed = inputs->pick_up_drop.pressed,
        .bomb_pressed = inputs->bomb.pressed,
        .rope_pressed = inputs->rope.pressed,
        .attack_pressed = inputs->attack.pressed,
        .buy_pressed = inputs->buy_button.pressed,
        .stop = inputs->stop.down,
        .no_hang = inputs->down.down,
    };
}

} // namespace splonks::controls

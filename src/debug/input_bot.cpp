#include "debug/input_bot.hpp"

#include "player_registry.hpp"
#include "utils.hpp"

namespace splonks::debug {

namespace {

ButtonState MakeDebugBotButtonState(bool down, bool previous_down) {
    return ButtonState{
        .down = down,
        .pressed = down && !previous_down,
        .released = !down && previous_down,
    };
}

} // namespace

PlayingInputs MakeDebugBotInputs(DebugLocalPlayerBot& bot) {
    if (bot.retarget_frames <= 0) {
        bot.move_dir = rng::RandomIntInclusive(-1, 1);
        bot.retarget_frames = rng::RandomIntInclusive(24, 90);
    } else {
        bot.retarget_frames -= 1;
    }

    bool jump_down = false;
    if (bot.jump_cooldown_frames > 0) {
        bot.jump_cooldown_frames -= 1;
    } else if (bot.allow_jump && rng::RandomIntInclusive(1, 45) == 1) {
        jump_down = true;
        bot.jump_cooldown_frames = rng::RandomIntInclusive(20, 70);
    }

    const bool left_down = bot.enabled && bot.move_dir < 0;
    const bool right_down = bot.enabled && bot.move_dir > 0;
    const bool run_down = bot.enabled && rng::RandomIntInclusive(1, 5) == 1;

    PlayingInputs inputs = PlayingInputs::New();
    inputs.left = MakeDebugBotButtonState(left_down, bot.previous_inputs.left.down);
    inputs.right = MakeDebugBotButtonState(right_down, bot.previous_inputs.right.down);
    inputs.jump = MakeDebugBotButtonState(bot.enabled && jump_down, bot.previous_inputs.jump.down);
    inputs.run = MakeDebugBotButtonState(run_down, bot.previous_inputs.run.down);
    if (bot.allow_tools) {
        const bool attack_down = bot.enabled && rng::RandomIntInclusive(1, 180) == 1;
        const bool bomb_down = bot.enabled && rng::RandomIntInclusive(1, 420) == 1;
        const bool rope_down = bot.enabled && rng::RandomIntInclusive(1, 420) == 1;
        inputs.attack = MakeDebugBotButtonState(attack_down, bot.previous_inputs.attack.down);
        inputs.bomb = MakeDebugBotButtonState(bomb_down, bot.previous_inputs.bomb.down);
        inputs.rope = MakeDebugBotButtonState(rope_down, bot.previous_inputs.rope.down);
    }
    inputs.mouse_pos = bot.previous_inputs.mouse_pos;
    bot.previous_inputs = inputs;
    return inputs;
}

void ApplyDebugPrimaryPlayerBotInput(State& state) {
    if (!state.debug_primary_player_bot_enabled) {
        return;
    }

    PlayerSlot* const primary = state.players.FindPrimaryLocal();
    if (primary == nullptr || primary->player_id == kInvalidPlayerId || !primary->connected) {
        return;
    }

    DebugLocalPlayerBot& bot = state.debug_primary_player_bot;
    bot.player_id = primary->player_id;
    bot.enabled = true;
    bot.allow_tools = false;
    const PlayingInputs inputs = MakeDebugBotInputs(bot);
    state.playing_input_snapshot = ToPlayingInputSnapshot(ToInputFrame(inputs));
    state.immediate_playing_inputs = inputs;
    state.players.SetPrimaryLocalInputs(inputs, inputs);
}

} // namespace splonks::debug

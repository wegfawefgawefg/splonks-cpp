#pragma once

#include "state.hpp"

namespace splonks::controls {

struct ControlIntent {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool jump_pressed = false;
    bool run = false;
    bool use_held = false;
    bool use_pressed = false;
    bool use_back = false;
    bool use_back_pressed = false;
    bool equip_pressed = false;
    bool pick_up_drop_pressed = false;
    bool bomb_pressed = false;
    bool rope_pressed = false;
    bool attack_pressed = false;
    bool buy_pressed = false;
    bool stop = false;
    bool no_hang = false;
};

ControlIntent GetControlIntentForEntity(const Entity& entity, const State& state);
void ControlEntityAsPlayer(const VID& entity_vid, State& state);

} // namespace splonks::controls

#pragma once

#include "hud/types.hpp"

#include <vector>

namespace splonks {

struct Entity;
struct State;

std::vector<HudEntry> BuildEffectHudEntries(const State& state, const Entity& player);
std::vector<HudEntry> BuildEquipmentHudEntries(const State& state, const Entity& player);

} // namespace splonks

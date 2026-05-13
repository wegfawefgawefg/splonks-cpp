#pragma once

#include "hud/types.hpp"

#include <vector>

namespace splonks {

struct Ent;
struct State;

std::vector<HudEntry> BuildEffectHudEntries(const State& state, const Ent& player);
std::vector<HudEntry> BuildEquipmentHudEntries(const State& state, const Ent& player);

} // namespace splonks

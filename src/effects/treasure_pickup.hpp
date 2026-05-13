#pragma once

#include "math_types.hpp"

namespace splonks {

struct Ent;
struct State;

namespace effects {

void SpawnTreasurePickupSparkles(const Ent& pickup, State& state, Color3 color, int count);

} // namespace effects

} // namespace splonks


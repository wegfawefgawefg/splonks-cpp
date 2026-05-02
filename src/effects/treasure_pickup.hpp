#pragma once

#include "math_types.hpp"

namespace splonks {

struct Entity;
struct State;

namespace effects {

void SpawnTreasurePickupSparkles(const Entity& pickup, State& state, Color3 color, int count);

} // namespace effects

} // namespace splonks


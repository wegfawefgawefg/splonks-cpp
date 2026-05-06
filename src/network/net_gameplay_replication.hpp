#pragma once

#include "gameplay_events.hpp"

namespace splonks {

struct State;

namespace network {

void ReplicateGameplayEvent(State& state, const GameplayEvent& event);

} // namespace network
} // namespace splonks

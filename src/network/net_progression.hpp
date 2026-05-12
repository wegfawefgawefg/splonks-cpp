#pragma once

#include "network/net_transport.hpp"
#include "stage.hpp"

namespace splonks {

struct Graphics;
struct State;

namespace network {

void NotifyStageLoaded(State& state);

} // namespace network
} // namespace splonks

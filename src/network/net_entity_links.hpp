#pragma once

#include "network/net_ids.hpp"
#include "vid.hpp"

namespace splonks {

struct State;

namespace network {

void RegisterStageEntityLinks(State& state);
NetEntityId GetOrAssignReplicatedEntityId(State& state, VID entity_vid);

} // namespace network
} // namespace splonks

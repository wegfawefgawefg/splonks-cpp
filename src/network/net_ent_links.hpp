#pragma once

#include "network/net_ids.hpp"
#include "vid.hpp"

namespace splonks {

struct State;

namespace network {

void RegisterStageEntLinks(State& state);
NetEntId GetOrAssignReplicatedEntId(State& state, VID ent_vid);

} // namespace network
} // namespace splonks

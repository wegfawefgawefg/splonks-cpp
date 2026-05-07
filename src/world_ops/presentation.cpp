#include "world_ops.hpp"

#include "network/net_gameplay_replication.hpp"
#include "state.hpp"

namespace splonks::world_ops {

void QueuePresentationCommand(State& state, const PresentationCommand& command) {
    network::ReplicatePresentationCommand(state, command);
}

} // namespace splonks::world_ops

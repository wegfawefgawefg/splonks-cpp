#include "network/net_lobby.hpp"

#include "graphics.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_progression.hpp"
#include "state.hpp"

namespace splonks::network {

void StepNetworkLobby(State& state, const Graphics& graphics) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }
    NetTransportRuntime& transport = *state.net_transport;
    transport.fuzzer_config = state.net_session.fuzzer_config;
    FlushFuzzedOutgoingPackets(transport);

    if (IsInputLockstepActive(state)) {
        if (state.net_session.role == NetRole::Coordinator) {
            CleanupExpiredRetainedPlayerStates(state);
            StepHostPackets(state, graphics, transport);
        } else if (state.net_session.role == NetRole::Peer) {
            StepPeerPackets(state, graphics, transport);
        }
        FlushFuzzedOutgoingPackets(transport);
        state.net_session.fuzzer_stats = transport.fuzzer_stats;
        return;
    }

    if (state.net_session.role == NetRole::Coordinator) {
        CleanupExpiredRetainedPlayerStates(state);
        StepHostPackets(state, graphics, transport);
        const bool should_send = ShouldSendSnapshots(state, transport);
        if (should_send) {
            SendStageSyncToAllRemotes(state, transport);
            SendSnapshotsToAllRemotes(state, transport);
            SendReplicatedEntityStatePatchesToAllRemotes(state, transport);
            SendReplicatedFluidCellPatchesToAllRemotes(state, transport);
            SendCoordinatorEntityRepairPatchesToAllRemotes(state, transport);
            SendOrderedMessagesToAllRemotes(state, transport);
        }
    } else if (state.net_session.role == NetRole::Peer) {
        StepPeerPackets(state, graphics, transport);
        const bool should_send = ShouldSendSnapshots(state, transport);
        if (!transport.join_request_pending && should_send) {
            SendSnapshotsToEndpoint(
                state,
                transport,
                transport.coordinator_endpoint
            );
            SendPendingPeerMessagesToCoordinator(state, transport);
            SendDurableMessageAckToCoordinator(state, transport);
        }
    }
    FlushFuzzedOutgoingPackets(transport);
    state.net_session.fuzzer_stats = transport.fuzzer_stats;
    StepRemotePlayerInterpolation(state, transport, graphics);
    SyncNetworkAttachmentsAfterRemoteMovement(state, graphics);
}

bool IsTransportOpen(const State& state) {
    return state.net_transport && state.net_transport->socket.IsOpen();
}

std::uint16_t BoundTransportPort(const State& state) {
    if (!IsTransportOpen(state)) {
        return 0;
    }
    return state.net_transport->socket.BoundPort();
}

} // namespace splonks::network

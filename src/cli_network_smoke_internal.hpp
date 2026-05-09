#pragma once

#include "audio.hpp"
#include "entity.hpp"
#include "network/net_event.hpp"
#include "network/net_lobby_internal.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"

#include <optional>
#include <vector>

namespace splonks {

struct PacketDeliveryPlan {
    bool reverse_order = false;
    bool duplicate_each_packet = false;
    bool drop_action_ack = false;
};

void InitNetworkSmokeRuntimeTables(Graphics& graphics);

const Entity* FindFirstActiveEntity(const State& state);
const Entity* FindFirstPlayerLikeEntity(const State& state);
std::optional<std::size_t> FindFirstUsableToolSlot(const State& state, VID owner_vid);

void ConfigureProtocolSmokeCoordinator(State& state);
void ConfigureProtocolSmokePeer(State& state);

bool ApplyCoordinatorEventsToPeer(
    State& coordinator,
    State& peer,
    const char* label,
    Audio* audio = nullptr,
    Graphics* graphics = nullptr
);

bool CompareProtocolSmokeStates(const State& coordinator, const State& peer, const char* label);

void LinkMatchingEntitiesForActionSmoke(State& coordinator, State& peer);

std::optional<VID> FindPeerEntityForCoordinatorEntity(
    const State& coordinator,
    const State& peer,
    VID coordinator_vid
);

bool RunPeerActionThroughCoordinator(
    State& coordinator,
    State& peer,
    const GameplayActionRequested& peer_action,
    Graphics& graphics,
    Audio& audio,
    const char* label
);

std::vector<network::UdpPacket> TakeCapturedPackets(network::NetTransportRuntime& transport);

bool DeliverPeerPacketsToCoordinator(
    State& peer,
    State& coordinator,
    network::NetTransportRuntime& peer_transport,
    network::NetTransportRuntime& coordinator_transport,
    const network::NetEndpoint& peer_endpoint,
    const char* label,
    const PacketDeliveryPlan& delivery_plan = PacketDeliveryPlan{}
);

bool DeliverCoordinatorPacketsToPeer(
    State& coordinator,
    State& peer,
    network::NetTransportRuntime& coordinator_transport,
    network::NetTransportRuntime& peer_transport,
    const network::NetEndpoint& peer_endpoint,
    Graphics& graphics,
    Audio& audio,
    const char* label,
    bool compare_after_delivery = true,
    const PacketDeliveryPlan& delivery_plan = PacketDeliveryPlan{}
);

bool DropCoordinatorPacketsToPeer(
    State& coordinator,
    network::NetTransportRuntime& coordinator_transport,
    const char* label
);

bool RunPeerActionThroughPacketCoordinator(
    State& coordinator,
    State& peer,
    network::NetTransportRuntime& coordinator_transport,
    network::NetTransportRuntime& peer_transport,
    const network::NetEndpoint& peer_endpoint,
    const GameplayActionRequested& peer_action,
    Graphics& graphics,
    Audio& audio,
    const char* label,
    const PacketDeliveryPlan& coordinator_delivery_plan = PacketDeliveryPlan{}
);

} // namespace splonks

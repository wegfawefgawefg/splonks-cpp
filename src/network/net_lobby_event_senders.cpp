#include "network/net_lobby_internal.hpp"

#include <vector>

namespace splonks::network {

void SendTileEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    TileEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedTileEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeTileEvents(packet));
            packet = TileEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeTileEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeTileEvents(packet));
    }
}

void SendFluidCellEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    FluidCellEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedFluidCellEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeFluidCellEvents(packet));
            packet = FluidCellEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeFluidCellEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeFluidCellEvents(packet));
    }
}

void SendEntitySpawnedEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntitySpawnedEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntitySpawnedEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedEvents(packet));
            packet = EntitySpawnedEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntitySpawnedEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedEvents(packet));
    }
}

void SendEntityDamageEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityDamageEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityDamageEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityDamageEvents(packet));
            packet = EntityDamageEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityDamageEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityDamageEvents(packet));
    }
}

void SendEntityStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityStateEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityStateEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityStateEvents(packet));
            packet = EntityStateEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityStateEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityStateEvents(packet));
    }
}

void SendEntityCarryEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityCarryEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityCarryEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityCarryEvents(packet));
            packet = EntityCarryEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityCarryEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityCarryEvents(packet));
    }
}

void SendEntityLifecycleEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityLifecycleEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityLifecycleEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityLifecycleEvents(packet));
            packet = EntityLifecycleEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityLifecycleEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityLifecycleEvents(packet));
    }
}

void SendPlayerStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    PlayerStateEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedPlayerStateEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodePlayerStateEvents(packet));
            packet = PlayerStateEventsPacket{};
        }
        packet.events[packet.event_count++] = MakePlayerStateEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodePlayerStateEvents(packet));
    }
}

void SendRunStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    RunStateEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedRunStateEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeRunStateEvents(packet));
            packet = RunStateEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeRunStateEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeRunStateEvents(packet));
    }
}

void SendPresentationCommandEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    PresentationCommandEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedPresentationCommandEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodePresentationCommandEvents(packet));
            packet = PresentationCommandEventsPacket{};
        }
        packet.events[packet.event_count++] = MakePresentationCommandEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodePresentationCommandEvents(packet));
    }
}

void SendActionRequestEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    ActionRequestEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedActionRequestEvent(event)) {
            continue;
        }
        if (packet.events.size() >= kNetActionRequestEventsPerPacket) {
            SendEncodedPacket(transport, endpoint, EncodeActionRequestEvents(packet));
            packet = ActionRequestEventsPacket{};
        }
        packet.events.push_back(MakeActionRequestEventEntry(event));
    }
    if (!packet.events.empty()) {
        SendEncodedPacket(transport, endpoint, EncodeActionRequestEvents(packet));
    }
}

} // namespace splonks::network

#include "network/net_lobby_internal.hpp"

#include <vector>

namespace splonks::network {

void SendTileMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    TileMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedTileMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeTileMessages(packet));
            packet = TileMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeTileMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeTileMessages(packet));
    }
}

void SendFluidCellMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    FluidCellMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedFluidCellMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeFluidCellMessages(packet));
            packet = FluidCellMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeFluidCellMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeFluidCellMessages(packet));
    }
}

void SendStageLightMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    StageLightMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedStageLightMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeStageLightMessages(packet));
            packet = StageLightMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeStageLightMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeStageLightMessages(packet));
    }
}

void SendEntitySpawnedMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    EntitySpawnedMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedEntitySpawnedMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedMessages(packet));
            packet = EntitySpawnedMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeEntitySpawnedMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedMessages(packet));
    }
}

void SendEntityDamageMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    EntityDamageMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedEntityDamageMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityDamageMessages(packet));
            packet = EntityDamageMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeEntityDamageMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityDamageMessages(packet));
    }
}

void SendEntityStateMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    EntityStateMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedEntityStateMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityStateMessages(packet));
            packet = EntityStateMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeEntityStateMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityStateMessages(packet));
    }
}

void SendEntityCarryMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    EntityCarryMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedEntityCarryMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityCarryMessages(packet));
            packet = EntityCarryMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeEntityCarryMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityCarryMessages(packet));
    }
}

void SendEntityLifecycleMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    EntityLifecycleMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedEntityLifecycleMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityLifecycleMessages(packet));
            packet = EntityLifecycleMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeEntityLifecycleMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityLifecycleMessages(packet));
    }
}

void SendPlayerStateMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    PlayerStateMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedPlayerStateMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodePlayerStateMessages(packet));
            packet = PlayerStateMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakePlayerStateMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodePlayerStateMessages(packet));
    }
}

void SendRunStateMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    RunStateMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedRunStateMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodeRunStateMessages(packet));
            packet = RunStateMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakeRunStateMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeRunStateMessages(packet));
    }
}

void SendPresentationCommandMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    PresentationCommandMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedPresentationCommandMessage(message)) {
            continue;
        }
        if (packet.message_count >= packet.messages.size()) {
            SendEncodedPacket(transport, endpoint, EncodePresentationCommandMessages(packet));
            packet = PresentationCommandMessagesPacket{};
        }
        packet.messages[packet.message_count++] = MakePresentationCommandMessageEntry(message);
    }
    if (packet.message_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodePresentationCommandMessages(packet));
    }
}

void SendActionRequestMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
) {
    ActionRequestMessagesPacket packet;
    for (const NetMessage& message : messages) {
        if (!IsReplicatedActionRequestMessage(message)) {
            continue;
        }
        if (packet.messages.size() >= kNetActionRequestMessagesPerPacket) {
            SendEncodedPacket(transport, endpoint, EncodeActionRequestMessages(packet));
            packet = ActionRequestMessagesPacket{};
        }
        packet.messages.push_back(MakeActionRequestMessageEntry(message));
    }
    if (!packet.messages.empty()) {
        SendEncodedPacket(transport, endpoint, EncodeActionRequestMessages(packet));
    }
}

} // namespace splonks::network

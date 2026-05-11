#include "network/net_protocol.hpp"

#include "network/net_message.hpp"

#include <cstring>

namespace splonks::network {

static_assert(sizeof(EntityDamageMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(FluidCellMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(StageLightMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntitySpawnedMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityStateMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityCarryMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityLifecycleMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PlayerStateMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(RunStateMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PresentationCommandMessagesPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(ActionRequestAckPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PlayerSnapshotsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(JoinRequestPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(LeaveNoticePacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(StageSyncPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(DurableMessageAckPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));

namespace {

template <typename T>
bool Append(EncodedNetPacket& encoded, const T& value) {
    if (encoded.size + sizeof(T) > encoded.bytes.size()) {
        return false;
    }
    std::memcpy(encoded.bytes.data() + encoded.size, &value, sizeof(T));
    encoded.size += sizeof(T);
    return true;
}

template <typename T>
bool Read(const std::uint8_t* bytes, std::size_t size, std::size_t& offset, T& out) {
    if (offset + sizeof(T) > size) {
        return false;
    }
    std::memcpy(&out, bytes + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

template <typename T>
EncodedNetPacket EncodePayload(NetPacketType type, const T& payload) {
    EncodedNetPacket encoded;
    NetPacketHeader header;
    header.type = type;
    header.payload_bytes = static_cast<std::uint16_t>(sizeof(T));
    (void)Append(encoded, header);
    (void)Append(encoded, payload);
    return encoded;
}

bool ReadHeader(
    const std::uint8_t* bytes,
    std::size_t size,
    NetPacketType expected_type,
    std::size_t& offset
) {
    NetPacketHeader header;
    if (!Read(bytes, size, offset, header)) {
        return false;
    }
    if (header.magic != kNetProtocolMagic || header.version != kNetProtocolVersion ||
        header.type != expected_type) {
        return false;
    }
    return offset + header.payload_bytes == size;
}

} // namespace

EncodedNetPacket EncodeJoinRequest(const JoinRequestPacket& packet) {
    return EncodePayload(NetPacketType::JoinRequest, packet);
}

EncodedNetPacket EncodeJoinAccept(const JoinAcceptPacket& packet) {
    return EncodePayload(NetPacketType::JoinAccept, packet);
}

EncodedNetPacket EncodePlayerSnapshots(const PlayerSnapshotsPacket& packet) {
    return EncodePayload(NetPacketType::PlayerSnapshots, packet);
}

EncodedNetPacket EncodeTileMessages(const TileMessagesPacket& packet) {
    return EncodePayload(NetPacketType::TileMessages, packet);
}

EncodedNetPacket EncodeFluidCellMessages(const FluidCellMessagesPacket& packet) {
    return EncodePayload(NetPacketType::FluidCellMessages, packet);
}

EncodedNetPacket EncodeStageLightMessages(const StageLightMessagesPacket& packet) {
    return EncodePayload(NetPacketType::StageLightMessages, packet);
}

EncodedNetPacket EncodeEntitySpawnedMessages(const EntitySpawnedMessagesPacket& packet) {
    return EncodePayload(NetPacketType::EntitySpawnedMessages, packet);
}

EncodedNetPacket EncodeEntityDamageMessages(const EntityDamageMessagesPacket& packet) {
    return EncodePayload(NetPacketType::EntityDamageMessages, packet);
}

EncodedNetPacket EncodeEntityStateMessages(const EntityStateMessagesPacket& packet) {
    return EncodePayload(NetPacketType::EntityStateMessages, packet);
}

EncodedNetPacket EncodeEntityCarryMessages(const EntityCarryMessagesPacket& packet) {
    return EncodePayload(NetPacketType::EntityCarryMessages, packet);
}

EncodedNetPacket EncodeEntityLifecycleMessages(const EntityLifecycleMessagesPacket& packet) {
    return EncodePayload(NetPacketType::EntityLifecycleMessages, packet);
}

EncodedNetPacket EncodePlayerStateMessages(const PlayerStateMessagesPacket& packet) {
    return EncodePayload(NetPacketType::PlayerStateMessages, packet);
}

EncodedNetPacket EncodeRunStateMessages(const RunStateMessagesPacket& packet) {
    return EncodePayload(NetPacketType::RunStateMessages, packet);
}

EncodedNetPacket EncodePresentationCommandMessages(const PresentationCommandMessagesPacket& packet) {
    return EncodePayload(NetPacketType::PresentationCommandMessages, packet);
}

EncodedNetPacket EncodeActionRequestMessages(const ActionRequestMessagesPacket& packet) {
    EncodedNetPacket encoded;
    NetPacketHeader header;
    header.type = NetPacketType::ActionRequestMessages;
    (void)Append(encoded, header);
    const std::size_t payload_start = encoded.size;

    const std::uint32_t message_count = static_cast<std::uint32_t>(packet.messages.size());
    if (!Append(encoded, message_count)) {
        return EncodedNetPacket{};
    }

    for (const ActionRequestMessageEntry& message : packet.messages) {
        if (!Append(encoded, message.message_id) ||
            !Append(encoded, message.source_player_id) ||
            !Append(encoded, message.stage_instance_id) ||
            !Append(encoded, message.source_local_frame) ||
            !Append(encoded, message.action_kind)) {
            return EncodedNetPacket{};
        }

        const NetActionKind action_kind = static_cast<NetActionKind>(message.action_kind);
        switch (action_kind) {
        case NetActionKind::UseTool:
            if (!Append(encoded, message.source_entity_id) ||
                !Append(encoded, message.velocity_x) ||
                !Append(encoded, message.velocity_y) ||
                !Append(encoded, message.tool_slot)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::PickupEntity:
        case NetActionKind::DropEntity:
        case NetActionKind::PutHeldEntityOnBack:
        case NetActionKind::TakeOffBackEntity:
        case NetActionKind::InteractEntity:
        case NetActionKind::CollectEntity:
            if (!Append(encoded, message.source_entity_id) ||
                !Append(encoded, message.target_entity_id)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::ThrowEntity:
        case NetActionKind::PushEntity:
            if (!Append(encoded, message.source_entity_id) ||
                !Append(encoded, message.target_entity_id) ||
                !Append(encoded, message.velocity_x) ||
                !Append(encoded, message.velocity_y)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::UseHeldEntity:
        case NetActionKind::UseBackEntity:
            if (!Append(encoded, message.source_entity_id) ||
                !Append(encoded, message.target_entity_id) ||
                !Append(encoded, message.direction_x) ||
                !Append(encoded, message.direction_y) ||
                !Append(encoded, message.use_edge)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::BreakTile:
            if (!Append(encoded, message.source_entity_id) ||
                !Append(encoded, message.tile_x) ||
                !Append(encoded, message.tile_y)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::DamageEntity:
            if (!Append(encoded, message.source_entity_id) ||
                !Append(encoded, message.target_entity_id) ||
                !Append(encoded, message.damage_type) ||
                !Append(encoded, message.amount)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::HitEntity:
            if (!Append(encoded, message.source_entity_id) ||
                !Append(encoded, message.target_entity_id) ||
                !Append(encoded, message.velocity_x) ||
                !Append(encoded, message.velocity_y) ||
                !Append(encoded, message.damage_type) ||
                !Append(encoded, message.projectile_contact_damage_type) ||
                !Append(encoded, message.amount) ||
                !Append(encoded, message.projectile_contact_damage_amount) ||
                !Append(encoded, message.thrown_immunity_timer) ||
                !Append(encoded, message.projectile_contact_duration) ||
                !Append(encoded, message.flags)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::None:
            break;
        }
    }

    header.payload_bytes = static_cast<std::uint16_t>(encoded.size - payload_start);
    std::memcpy(encoded.bytes.data(), &header, sizeof(header));
    return encoded;
}

EncodedNetPacket EncodeActionRequestAck(const ActionRequestAckPacket& packet) {
    return EncodePayload(NetPacketType::ActionRequestAck, packet);
}

EncodedNetPacket EncodeLeaveNotice(const LeaveNoticePacket& packet) {
    return EncodePayload(NetPacketType::LeaveNotice, packet);
}

EncodedNetPacket EncodeStageSync(const StageSyncPacket& packet) {
    return EncodePayload(NetPacketType::StageSync, packet);
}

EncodedNetPacket EncodeDurableMessageAck(const DurableMessageAckPacket& packet) {
    return EncodePayload(NetPacketType::DurableMessageAck, packet);
}

std::optional<JoinRequestPacket> TryDecodeJoinRequest(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::JoinRequest, offset)) {
        return std::nullopt;
    }
    JoinRequestPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

std::optional<JoinAcceptPacket> TryDecodeJoinAccept(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::JoinAccept, offset)) {
        return std::nullopt;
    }
    JoinAcceptPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

std::optional<PlayerSnapshotsPacket> TryDecodePlayerSnapshots(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::PlayerSnapshots, offset)) {
        return std::nullopt;
    }
    PlayerSnapshotsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.snapshot_count = std::min<std::uint32_t>(
        packet.snapshot_count,
        static_cast<std::uint32_t>(packet.snapshots.size())
    );
    return packet;
}

std::optional<TileMessagesPacket> TryDecodeTileMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::TileMessages, offset)) {
        return std::nullopt;
    }
    TileMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<FluidCellMessagesPacket> TryDecodeFluidCellMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::FluidCellMessages, offset)) {
        return std::nullopt;
    }
    FluidCellMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<StageLightMessagesPacket> TryDecodeStageLightMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::StageLightMessages, offset)) {
        return std::nullopt;
    }
    StageLightMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<EntitySpawnedMessagesPacket> TryDecodeEntitySpawnedMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntitySpawnedMessages, offset)) {
        return std::nullopt;
    }
    EntitySpawnedMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<EntityDamageMessagesPacket> TryDecodeEntityDamageMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityDamageMessages, offset)) {
        return std::nullopt;
    }
    EntityDamageMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<EntityStateMessagesPacket> TryDecodeEntityStateMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityStateMessages, offset)) {
        return std::nullopt;
    }
    EntityStateMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<EntityCarryMessagesPacket> TryDecodeEntityCarryMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityCarryMessages, offset)) {
        return std::nullopt;
    }
    EntityCarryMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<EntityLifecycleMessagesPacket> TryDecodeEntityLifecycleMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityLifecycleMessages, offset)) {
        return std::nullopt;
    }
    EntityLifecycleMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<PlayerStateMessagesPacket> TryDecodePlayerStateMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::PlayerStateMessages, offset)) {
        return std::nullopt;
    }
    PlayerStateMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        packet.messages[i].effect_count = std::min<std::uint8_t>(
            packet.messages[i].effect_count,
            static_cast<std::uint8_t>(packet.messages[i].effects.size())
        );
    }
    return packet;
}

std::optional<RunStateMessagesPacket> TryDecodeRunStateMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::RunStateMessages, offset)) {
        return std::nullopt;
    }
    RunStateMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<PresentationCommandMessagesPacket> TryDecodePresentationCommandMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::PresentationCommandMessages, offset)) {
        return std::nullopt;
    }
    PresentationCommandMessagesPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.message_count = std::min<std::uint32_t>(
        packet.message_count,
        static_cast<std::uint32_t>(packet.messages.size())
    );
    return packet;
}

std::optional<ActionRequestMessagesPacket> TryDecodeActionRequestMessages(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::ActionRequestMessages, offset)) {
        return std::nullopt;
    }
    ActionRequestMessagesPacket packet;
    std::uint32_t message_count = 0;
    if (!Read(bytes, size, offset, message_count)) {
        return std::nullopt;
    }
    packet.messages.reserve(std::min<std::uint32_t>(message_count, kNetActionRequestMessagesPerPacket));
    for (std::uint32_t i = 0; i < message_count; ++i) {
        ActionRequestMessageEntry message;
        if (!Read(bytes, size, offset, message.message_id) ||
            !Read(bytes, size, offset, message.source_player_id) ||
            !Read(bytes, size, offset, message.stage_instance_id) ||
            !Read(bytes, size, offset, message.source_local_frame) ||
            !Read(bytes, size, offset, message.action_kind)) {
            return std::nullopt;
        }

        const NetActionKind action_kind = static_cast<NetActionKind>(message.action_kind);
        switch (action_kind) {
        case NetActionKind::UseTool:
            if (!Read(bytes, size, offset, message.source_entity_id) ||
                !Read(bytes, size, offset, message.velocity_x) ||
                !Read(bytes, size, offset, message.velocity_y) ||
                !Read(bytes, size, offset, message.tool_slot)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::PickupEntity:
        case NetActionKind::DropEntity:
        case NetActionKind::PutHeldEntityOnBack:
        case NetActionKind::TakeOffBackEntity:
        case NetActionKind::InteractEntity:
        case NetActionKind::CollectEntity:
            if (!Read(bytes, size, offset, message.source_entity_id) ||
                !Read(bytes, size, offset, message.target_entity_id)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::ThrowEntity:
        case NetActionKind::PushEntity:
            if (!Read(bytes, size, offset, message.source_entity_id) ||
                !Read(bytes, size, offset, message.target_entity_id) ||
                !Read(bytes, size, offset, message.velocity_x) ||
                !Read(bytes, size, offset, message.velocity_y)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::UseHeldEntity:
        case NetActionKind::UseBackEntity:
            if (!Read(bytes, size, offset, message.source_entity_id) ||
                !Read(bytes, size, offset, message.target_entity_id) ||
                !Read(bytes, size, offset, message.direction_x) ||
                !Read(bytes, size, offset, message.direction_y) ||
                !Read(bytes, size, offset, message.use_edge)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::BreakTile:
            if (!Read(bytes, size, offset, message.source_entity_id) ||
                !Read(bytes, size, offset, message.tile_x) ||
                !Read(bytes, size, offset, message.tile_y)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::DamageEntity:
            if (!Read(bytes, size, offset, message.source_entity_id) ||
                !Read(bytes, size, offset, message.target_entity_id) ||
                !Read(bytes, size, offset, message.damage_type) ||
                !Read(bytes, size, offset, message.amount)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::HitEntity:
            if (!Read(bytes, size, offset, message.source_entity_id) ||
                !Read(bytes, size, offset, message.target_entity_id) ||
                !Read(bytes, size, offset, message.velocity_x) ||
                !Read(bytes, size, offset, message.velocity_y) ||
                !Read(bytes, size, offset, message.damage_type) ||
                !Read(bytes, size, offset, message.projectile_contact_damage_type) ||
                !Read(bytes, size, offset, message.amount) ||
                !Read(bytes, size, offset, message.projectile_contact_damage_amount) ||
                !Read(bytes, size, offset, message.thrown_immunity_timer) ||
                !Read(bytes, size, offset, message.projectile_contact_duration) ||
                !Read(bytes, size, offset, message.flags)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::None:
            break;
        }
        packet.messages.push_back(message);
    }
    if (offset != size) {
        return std::nullopt;
    }
    return packet;
}

std::optional<ActionRequestAckPacket> TryDecodeActionRequestAck(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::ActionRequestAck, offset)) {
        return std::nullopt;
    }
    ActionRequestAckPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.ack_count = std::min<std::uint32_t>(
        packet.ack_count,
        static_cast<std::uint32_t>(packet.message_ids.size())
    );
    return packet;
}

std::optional<LeaveNoticePacket> TryDecodeLeaveNotice(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::LeaveNotice, offset)) {
        return std::nullopt;
    }
    LeaveNoticePacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.player_count = std::min<std::uint32_t>(
        packet.player_count,
        static_cast<std::uint32_t>(packet.player_ids.size())
    );
    return packet;
}

std::optional<StageSyncPacket> TryDecodeStageSync(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::StageSync, offset)) {
        return std::nullopt;
    }
    StageSyncPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

std::optional<DurableMessageAckPacket> TryDecodeDurableMessageAck(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::DurableMessageAck, offset)) {
        return std::nullopt;
    }
    DurableMessageAckPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

} // namespace splonks::network

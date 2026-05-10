#include "network/net_protocol.hpp"

#include "network/net_event.hpp"

#include <cstring>

namespace splonks::network {

static_assert(sizeof(EntityDamageEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(FluidCellEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntitySpawnedEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityStateEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityCarryEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityLifecycleEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PlayerStateEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(RunStateEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PresentationCommandEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(ActionRequestAckPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PlayerSnapshotsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(JoinRequestPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(LeaveNoticePacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(StageSyncPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(DurableEventAckPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));

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

EncodedNetPacket EncodeTileEvents(const TileEventsPacket& packet) {
    return EncodePayload(NetPacketType::TileEvents, packet);
}

EncodedNetPacket EncodeFluidCellEvents(const FluidCellEventsPacket& packet) {
    return EncodePayload(NetPacketType::FluidCellEvents, packet);
}

EncodedNetPacket EncodeEntitySpawnedEvents(const EntitySpawnedEventsPacket& packet) {
    return EncodePayload(NetPacketType::EntitySpawnedEvents, packet);
}

EncodedNetPacket EncodeEntityDamageEvents(const EntityDamageEventsPacket& packet) {
    return EncodePayload(NetPacketType::EntityDamageEvents, packet);
}

EncodedNetPacket EncodeEntityStateEvents(const EntityStateEventsPacket& packet) {
    return EncodePayload(NetPacketType::EntityStateEvents, packet);
}

EncodedNetPacket EncodeEntityCarryEvents(const EntityCarryEventsPacket& packet) {
    return EncodePayload(NetPacketType::EntityCarryEvents, packet);
}

EncodedNetPacket EncodeEntityLifecycleEvents(const EntityLifecycleEventsPacket& packet) {
    return EncodePayload(NetPacketType::EntityLifecycleEvents, packet);
}

EncodedNetPacket EncodePlayerStateEvents(const PlayerStateEventsPacket& packet) {
    return EncodePayload(NetPacketType::PlayerStateEvents, packet);
}

EncodedNetPacket EncodeRunStateEvents(const RunStateEventsPacket& packet) {
    return EncodePayload(NetPacketType::RunStateEvents, packet);
}

EncodedNetPacket EncodePresentationCommandEvents(const PresentationCommandEventsPacket& packet) {
    return EncodePayload(NetPacketType::PresentationCommandEvents, packet);
}

EncodedNetPacket EncodeActionRequestEvents(const ActionRequestEventsPacket& packet) {
    EncodedNetPacket encoded;
    NetPacketHeader header;
    header.type = NetPacketType::ActionRequestEvents;
    (void)Append(encoded, header);
    const std::size_t payload_start = encoded.size;

    const std::uint32_t event_count = static_cast<std::uint32_t>(packet.events.size());
    if (!Append(encoded, event_count)) {
        return EncodedNetPacket{};
    }

    for (const ActionRequestEventEntry& event : packet.events) {
        if (!Append(encoded, event.event_id) ||
            !Append(encoded, event.source_player_id) ||
            !Append(encoded, event.stage_instance_id) ||
            !Append(encoded, event.source_local_frame) ||
            !Append(encoded, event.action_kind)) {
            return EncodedNetPacket{};
        }

        const NetActionKind action_kind = static_cast<NetActionKind>(event.action_kind);
        switch (action_kind) {
        case NetActionKind::UseTool:
            if (!Append(encoded, event.source_entity_id) ||
                !Append(encoded, event.velocity_x) ||
                !Append(encoded, event.velocity_y) ||
                !Append(encoded, event.tool_slot)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::PickupEntity:
        case NetActionKind::DropEntity:
        case NetActionKind::PutHeldEntityOnBack:
        case NetActionKind::TakeOffBackEntity:
        case NetActionKind::InteractEntity:
        case NetActionKind::CollectEntity:
            if (!Append(encoded, event.source_entity_id) ||
                !Append(encoded, event.target_entity_id)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::ThrowEntity:
        case NetActionKind::PushEntity:
            if (!Append(encoded, event.source_entity_id) ||
                !Append(encoded, event.target_entity_id) ||
                !Append(encoded, event.velocity_x) ||
                !Append(encoded, event.velocity_y)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::UseHeldEntity:
        case NetActionKind::UseBackEntity:
            if (!Append(encoded, event.source_entity_id) ||
                !Append(encoded, event.target_entity_id) ||
                !Append(encoded, event.direction_x) ||
                !Append(encoded, event.direction_y) ||
                !Append(encoded, event.use_edge)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::BreakTile:
            if (!Append(encoded, event.source_entity_id) ||
                !Append(encoded, event.tile_x) ||
                !Append(encoded, event.tile_y)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::DamageEntity:
            if (!Append(encoded, event.source_entity_id) ||
                !Append(encoded, event.target_entity_id) ||
                !Append(encoded, event.damage_type) ||
                !Append(encoded, event.amount)) {
                return EncodedNetPacket{};
            }
            break;
        case NetActionKind::HitEntity:
            if (!Append(encoded, event.source_entity_id) ||
                !Append(encoded, event.target_entity_id) ||
                !Append(encoded, event.velocity_x) ||
                !Append(encoded, event.velocity_y) ||
                !Append(encoded, event.damage_type) ||
                !Append(encoded, event.projectile_contact_damage_type) ||
                !Append(encoded, event.amount) ||
                !Append(encoded, event.projectile_contact_damage_amount) ||
                !Append(encoded, event.thrown_immunity_timer) ||
                !Append(encoded, event.projectile_contact_duration) ||
                !Append(encoded, event.flags)) {
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

EncodedNetPacket EncodeDurableEventAck(const DurableEventAckPacket& packet) {
    return EncodePayload(NetPacketType::DurableEventAck, packet);
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

std::optional<TileEventsPacket> TryDecodeTileEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::TileEvents, offset)) {
        return std::nullopt;
    }
    TileEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<FluidCellEventsPacket> TryDecodeFluidCellEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::FluidCellEvents, offset)) {
        return std::nullopt;
    }
    FluidCellEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<EntitySpawnedEventsPacket> TryDecodeEntitySpawnedEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntitySpawnedEvents, offset)) {
        return std::nullopt;
    }
    EntitySpawnedEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<EntityDamageEventsPacket> TryDecodeEntityDamageEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityDamageEvents, offset)) {
        return std::nullopt;
    }
    EntityDamageEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<EntityStateEventsPacket> TryDecodeEntityStateEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityStateEvents, offset)) {
        return std::nullopt;
    }
    EntityStateEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<EntityCarryEventsPacket> TryDecodeEntityCarryEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityCarryEvents, offset)) {
        return std::nullopt;
    }
    EntityCarryEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<EntityLifecycleEventsPacket> TryDecodeEntityLifecycleEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::EntityLifecycleEvents, offset)) {
        return std::nullopt;
    }
    EntityLifecycleEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<PlayerStateEventsPacket> TryDecodePlayerStateEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::PlayerStateEvents, offset)) {
        return std::nullopt;
    }
    PlayerStateEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        packet.events[i].effect_count = std::min<std::uint8_t>(
            packet.events[i].effect_count,
            static_cast<std::uint8_t>(packet.events[i].effects.size())
        );
    }
    return packet;
}

std::optional<RunStateEventsPacket> TryDecodeRunStateEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::RunStateEvents, offset)) {
        return std::nullopt;
    }
    RunStateEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<PresentationCommandEventsPacket> TryDecodePresentationCommandEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::PresentationCommandEvents, offset)) {
        return std::nullopt;
    }
    PresentationCommandEventsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.event_count = std::min<std::uint32_t>(
        packet.event_count,
        static_cast<std::uint32_t>(packet.events.size())
    );
    return packet;
}

std::optional<ActionRequestEventsPacket> TryDecodeActionRequestEvents(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::ActionRequestEvents, offset)) {
        return std::nullopt;
    }
    ActionRequestEventsPacket packet;
    std::uint32_t event_count = 0;
    if (!Read(bytes, size, offset, event_count)) {
        return std::nullopt;
    }
    packet.events.reserve(std::min<std::uint32_t>(event_count, kNetActionRequestEventsPerPacket));
    for (std::uint32_t i = 0; i < event_count; ++i) {
        ActionRequestEventEntry event;
        if (!Read(bytes, size, offset, event.event_id) ||
            !Read(bytes, size, offset, event.source_player_id) ||
            !Read(bytes, size, offset, event.stage_instance_id) ||
            !Read(bytes, size, offset, event.source_local_frame) ||
            !Read(bytes, size, offset, event.action_kind)) {
            return std::nullopt;
        }

        const NetActionKind action_kind = static_cast<NetActionKind>(event.action_kind);
        switch (action_kind) {
        case NetActionKind::UseTool:
            if (!Read(bytes, size, offset, event.source_entity_id) ||
                !Read(bytes, size, offset, event.velocity_x) ||
                !Read(bytes, size, offset, event.velocity_y) ||
                !Read(bytes, size, offset, event.tool_slot)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::PickupEntity:
        case NetActionKind::DropEntity:
        case NetActionKind::PutHeldEntityOnBack:
        case NetActionKind::TakeOffBackEntity:
        case NetActionKind::InteractEntity:
        case NetActionKind::CollectEntity:
            if (!Read(bytes, size, offset, event.source_entity_id) ||
                !Read(bytes, size, offset, event.target_entity_id)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::ThrowEntity:
        case NetActionKind::PushEntity:
            if (!Read(bytes, size, offset, event.source_entity_id) ||
                !Read(bytes, size, offset, event.target_entity_id) ||
                !Read(bytes, size, offset, event.velocity_x) ||
                !Read(bytes, size, offset, event.velocity_y)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::UseHeldEntity:
        case NetActionKind::UseBackEntity:
            if (!Read(bytes, size, offset, event.source_entity_id) ||
                !Read(bytes, size, offset, event.target_entity_id) ||
                !Read(bytes, size, offset, event.direction_x) ||
                !Read(bytes, size, offset, event.direction_y) ||
                !Read(bytes, size, offset, event.use_edge)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::BreakTile:
            if (!Read(bytes, size, offset, event.source_entity_id) ||
                !Read(bytes, size, offset, event.tile_x) ||
                !Read(bytes, size, offset, event.tile_y)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::DamageEntity:
            if (!Read(bytes, size, offset, event.source_entity_id) ||
                !Read(bytes, size, offset, event.target_entity_id) ||
                !Read(bytes, size, offset, event.damage_type) ||
                !Read(bytes, size, offset, event.amount)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::HitEntity:
            if (!Read(bytes, size, offset, event.source_entity_id) ||
                !Read(bytes, size, offset, event.target_entity_id) ||
                !Read(bytes, size, offset, event.velocity_x) ||
                !Read(bytes, size, offset, event.velocity_y) ||
                !Read(bytes, size, offset, event.damage_type) ||
                !Read(bytes, size, offset, event.projectile_contact_damage_type) ||
                !Read(bytes, size, offset, event.amount) ||
                !Read(bytes, size, offset, event.projectile_contact_damage_amount) ||
                !Read(bytes, size, offset, event.thrown_immunity_timer) ||
                !Read(bytes, size, offset, event.projectile_contact_duration) ||
                !Read(bytes, size, offset, event.flags)) {
                return std::nullopt;
            }
            break;
        case NetActionKind::None:
            break;
        }
        packet.events.push_back(event);
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
        static_cast<std::uint32_t>(packet.event_ids.size())
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

std::optional<DurableEventAckPacket> TryDecodeDurableEventAck(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::DurableEventAck, offset)) {
        return std::nullopt;
    }
    DurableEventAckPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

} // namespace splonks::network

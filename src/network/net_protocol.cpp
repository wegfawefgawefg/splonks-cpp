#include "network/net_protocol.hpp"

#include <cstring>

namespace splonks::network {

static_assert(sizeof(EntityDamageEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntitySpawnedEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityStateEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(EntityCarryEventsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(LeaveNoticePacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(StageSyncPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(StageExitRequestPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));

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

EncodedNetPacket EncodeLeaveNotice(const LeaveNoticePacket& packet) {
    return EncodePayload(NetPacketType::LeaveNotice, packet);
}

EncodedNetPacket EncodeStageSync(const StageSyncPacket& packet) {
    return EncodePayload(NetPacketType::StageSync, packet);
}

EncodedNetPacket EncodeStageExitRequest(const StageExitRequestPacket& packet) {
    return EncodePayload(NetPacketType::StageExitRequest, packet);
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

std::optional<StageExitRequestPacket> TryDecodeStageExitRequest(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::StageExitRequest, offset)) {
        return std::nullopt;
    }
    StageExitRequestPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

} // namespace splonks::network

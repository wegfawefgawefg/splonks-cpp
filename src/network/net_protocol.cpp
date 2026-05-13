#include "network/net_protocol.hpp"

#include <cstring>

namespace splonks::network {

static_assert(sizeof(JoinRequestPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(JoinAcceptPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PingPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(PongPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(LeaveNoticePacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(InputFrameRecordsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(LockstepSettingsPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(LockstepHashNetPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(SnapshotResyncRequestPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(SnapshotResyncChunkPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));
static_assert(sizeof(SnapshotResyncAckPacket) <= kNetPacketMaxBytes - sizeof(NetPacketHeader));

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

EncodedNetPacket EncodePing(const PingPacket& packet) {
    return EncodePayload(NetPacketType::Ping, packet);
}

EncodedNetPacket EncodePong(const PongPacket& packet) {
    return EncodePayload(NetPacketType::Pong, packet);
}

EncodedNetPacket EncodeLeaveNotice(const LeaveNoticePacket& packet) {
    return EncodePayload(NetPacketType::LeaveNotice, packet);
}

EncodedNetPacket EncodeInputFrameRecords(const InputFrameRecordsPacket& packet) {
    return EncodePayload(NetPacketType::InputFrameRecords, packet);
}

EncodedNetPacket EncodeLockstepSettings(const LockstepSettingsPacket& packet) {
    return EncodePayload(NetPacketType::LockstepSettings, packet);
}

EncodedNetPacket EncodeLockstepHash(const LockstepHashNetPacket& packet) {
    return EncodePayload(NetPacketType::LockstepHash, packet);
}

EncodedNetPacket EncodeSnapshotResyncRequest(const SnapshotResyncRequestPacket& packet) {
    return EncodePayload(NetPacketType::SnapshotResyncRequest, packet);
}

EncodedNetPacket EncodeSnapshotResyncChunk(const SnapshotResyncChunkPacket& packet) {
    return EncodePayload(NetPacketType::SnapshotResyncChunk, packet);
}

EncodedNetPacket EncodeSnapshotResyncAck(const SnapshotResyncAckPacket& packet) {
    return EncodePayload(NetPacketType::SnapshotResyncAck, packet);
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
    packet.local_player_count = std::clamp<std::uint32_t>(
        packet.local_player_count,
        1U,
        static_cast<std::uint32_t>(packet.preferred_player_ids.size())
    );
    packet.preferred_player_count = std::min<std::uint32_t>(
        packet.preferred_player_count,
        static_cast<std::uint32_t>(packet.preferred_player_ids.size())
    );
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
    packet.assigned_player_count = std::clamp<std::uint32_t>(
        packet.assigned_player_count,
        1U,
        static_cast<std::uint32_t>(packet.assigned_player_ids.size())
    );
    packet.lockstep_input_delay_frames =
        ClampLockstepInputDelayFrames(packet.lockstep_input_delay_frames);
    packet.lockstep_max_rollback_frames =
        ClampLockstepMaxRollbackFrames(packet.lockstep_max_rollback_frames);
    return packet;
}

std::optional<PingPacket> TryDecodePing(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::Ping, offset)) {
        return std::nullopt;
    }
    PingPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

std::optional<PongPacket> TryDecodePong(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::Pong, offset)) {
        return std::nullopt;
    }
    PongPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
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

std::optional<InputFrameRecordsPacket> TryDecodeInputFrameRecords(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::InputFrameRecords, offset)) {
        return std::nullopt;
    }
    InputFrameRecordsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.record_count = std::min<std::uint32_t>(
        packet.record_count,
        static_cast<std::uint32_t>(packet.records.size())
    );
    return packet;
}

std::optional<LockstepSettingsPacket> TryDecodeLockstepSettings(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::LockstepSettings, offset)) {
        return std::nullopt;
    }
    LockstepSettingsPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.input_delay_frames = ClampLockstepInputDelayFrames(packet.input_delay_frames);
    packet.max_rollback_frames = ClampLockstepMaxRollbackFrames(packet.max_rollback_frames);
    return packet;
}

std::optional<LockstepHashNetPacket> TryDecodeLockstepHash(const std::uint8_t* bytes, std::size_t size) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::LockstepHash, offset)) {
        return std::nullopt;
    }
    LockstepHashNetPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

std::optional<SnapshotResyncRequestPacket> TryDecodeSnapshotResyncRequest(
    const std::uint8_t* bytes,
    std::size_t size
) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::SnapshotResyncRequest, offset)) {
        return std::nullopt;
    }
    SnapshotResyncRequestPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

std::optional<SnapshotResyncChunkPacket> TryDecodeSnapshotResyncChunk(
    const std::uint8_t* bytes,
    std::size_t size
) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::SnapshotResyncChunk, offset)) {
        return std::nullopt;
    }
    SnapshotResyncChunkPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    packet.payload_bytes = std::min<std::uint32_t>(
        packet.payload_bytes,
        static_cast<std::uint32_t>(packet.payload.size())
    );
    return packet;
}

std::optional<SnapshotResyncAckPacket> TryDecodeSnapshotResyncAck(
    const std::uint8_t* bytes,
    std::size_t size
) {
    std::size_t offset = 0;
    if (!ReadHeader(bytes, size, NetPacketType::SnapshotResyncAck, offset)) {
        return std::nullopt;
    }
    SnapshotResyncAckPacket packet;
    if (!Read(bytes, size, offset, packet)) {
        return std::nullopt;
    }
    return packet;
}

} // namespace splonks::network

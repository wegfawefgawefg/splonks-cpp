#include "network/net_protocol.hpp"

#include <algorithm>
#include <cstring>

namespace splonks::network {

namespace {

constexpr std::size_t kMaxPacketPayloadBytes = 0xFFFFU;

class PacketWriter {
public:
    explicit PacketWriter(NetPacketType type) {
        WriteU32(kNetProtocolMagic);
        WriteU16(kNetProtocolVersion);
        WriteU16(static_cast<std::uint16_t>(type));
        payload_size_offset_ = encoded_.size;
        WriteU16(0);
        payload_start_ = encoded_.size;
    }

    EncodedNetPacket Finish() {
        if (!ok_) {
            return EncodedNetPacket{};
        }
        const std::size_t payload_bytes = encoded_.size - payload_start_;
        if (payload_bytes > kMaxPacketPayloadBytes) {
            return EncodedNetPacket{};
        }
        encoded_.bytes[payload_size_offset_] = static_cast<std::uint8_t>(payload_bytes & 0xFFU);
        encoded_.bytes[payload_size_offset_ + 1] = static_cast<std::uint8_t>((payload_bytes >> 8U) & 0xFFU);
        return encoded_;
    }

    void WriteU8(std::uint8_t value) {
        WriteByte(value);
    }

    void WriteU16(std::uint16_t value) {
        WriteByte(static_cast<std::uint8_t>(value & 0xFFU));
        WriteByte(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    }

    void WriteU32(std::uint32_t value) {
        for (unsigned int shift = 0; shift < 32; shift += 8) {
            WriteByte(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    void WriteU64(std::uint64_t value) {
        for (unsigned int shift = 0; shift < 64; shift += 8) {
            WriteByte(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    void WriteF32(float value) {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        WriteU32(bits);
    }

    template <std::size_t N>
    void WriteBytes(const std::array<std::uint8_t, N>& values, std::size_t count = N) {
        count = std::min(count, N);
        if (encoded_.size + count > encoded_.bytes.size()) {
            ok_ = false;
            return;
        }
        std::memcpy(encoded_.bytes.data() + encoded_.size, values.data(), count);
        encoded_.size += count;
    }

    template <std::size_t N>
    void WriteChars(const std::array<char, N>& values) {
        if (encoded_.size + N > encoded_.bytes.size()) {
            ok_ = false;
            return;
        }
        std::memcpy(encoded_.bytes.data() + encoded_.size, values.data(), N);
        encoded_.size += N;
    }

private:
    void WriteByte(std::uint8_t value) {
        if (encoded_.size >= encoded_.bytes.size()) {
            ok_ = false;
            return;
        }
        encoded_.bytes[encoded_.size++] = value;
    }

    EncodedNetPacket encoded_;
    std::size_t payload_size_offset_ = 0;
    std::size_t payload_start_ = 0;
    bool ok_ = true;
};

class PacketReader {
public:
    PacketReader(const std::uint8_t* bytes, std::size_t size, NetPacketType expected_type)
        : bytes_(bytes), size_(size) {
        const std::uint32_t magic = ReadU32();
        const std::uint16_t version = ReadU16();
        const auto type = static_cast<NetPacketType>(ReadU16());
        const std::uint16_t payload_bytes = ReadU16();
        if (!ok_ || magic != kNetProtocolMagic || version != kNetProtocolVersion ||
            type != expected_type || offset_ + payload_bytes != size_) {
            ok_ = false;
        }
    }

    bool Done() const {
        return ok_ && offset_ == size_;
    }

    std::uint8_t ReadU8() {
        if (offset_ + 1 > size_) {
            ok_ = false;
            return 0;
        }
        return bytes_[offset_++];
    }

    std::uint16_t ReadU16() {
        std::uint16_t value = 0;
        for (unsigned int shift = 0; shift < 16; shift += 8) {
            value = static_cast<std::uint16_t>(
                value | static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(ReadU8()) << shift
                )
            );
        }
        return value;
    }

    std::uint32_t ReadU32() {
        std::uint32_t value = 0;
        for (unsigned int shift = 0; shift < 32; shift += 8) {
            value |= static_cast<std::uint32_t>(ReadU8()) << shift;
        }
        return value;
    }

    std::uint64_t ReadU64() {
        std::uint64_t value = 0;
        for (unsigned int shift = 0; shift < 64; shift += 8) {
            value |= static_cast<std::uint64_t>(ReadU8()) << shift;
        }
        return value;
    }

    float ReadF32() {
        const std::uint32_t bits = ReadU32();
        float value = 0.0F;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    template <std::size_t N>
    void ReadBytes(std::array<std::uint8_t, N>& values, std::size_t count = N) {
        count = std::min(count, N);
        if (offset_ + count > size_) {
            ok_ = false;
            return;
        }
        std::memcpy(values.data(), bytes_ + offset_, count);
        offset_ += count;
    }

    template <std::size_t N>
    void ReadChars(std::array<char, N>& values) {
        if (offset_ + N > size_) {
            ok_ = false;
            return;
        }
        std::memcpy(values.data(), bytes_ + offset_, N);
        offset_ += N;
    }

private:
    const std::uint8_t* bytes_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
    bool ok_ = true;
};

void WritePlayerIds(PacketWriter& writer, const std::array<PlayerId, kNetPlayersPerProcess>& ids) {
    for (PlayerId id : ids) {
        writer.WriteU32(id);
    }
}

void ReadPlayerIds(PacketReader& reader, std::array<PlayerId, kNetPlayersPerProcess>& ids) {
    for (PlayerId& id : ids) {
        id = reader.ReadU32();
    }
}

void WriteFloats(PacketWriter& writer, const std::array<float, kNetPlayersPerProcess>& values) {
    for (float value : values) {
        writer.WriteF32(value);
    }
}

void ReadFloats(PacketReader& reader, std::array<float, kNetPlayersPerProcess>& values) {
    for (float& value : values) {
        value = reader.ReadF32();
    }
}

} // namespace

EncodedNetPacket EncodeJoinRequest(const JoinRequestPacket& packet) {
    PacketWriter writer(NetPacketType::JoinRequest);
    writer.WriteU32(packet.local_player_count);
    writer.WriteU32(packet.preferred_player_count);
    WritePlayerIds(writer, packet.preferred_player_ids);
    writer.WriteChars(packet.display_name);
    return writer.Finish();
}

EncodedNetPacket EncodeJoinAccept(const JoinAcceptPacket& packet) {
    PacketWriter writer(NetPacketType::JoinAccept);
    writer.WriteU32(packet.assigned_player_count);
    WritePlayerIds(writer, packet.assigned_player_ids);
    writer.WriteU32(packet.host_player_id);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteF32(packet.remote_spawn_x);
    writer.WriteF32(packet.remote_spawn_y);
    writer.WriteF32(packet.host_spawn_x);
    writer.WriteF32(packet.host_spawn_y);
    writer.WriteU32(packet.stage_seed);
    writer.WriteU64(packet.lockstep_start_frame);
    writer.WriteU32(packet.lockstep_input_delay_frames);
    writer.WriteU32(packet.lockstep_max_rollback_frames);
    writer.WriteU8(packet.multiplayer_respawn_mode);
    writer.WriteChars(packet.quest_id);
    writer.WriteChars(packet.quest_stage_id);
    writer.WriteChars(packet.host_name);
    return writer.Finish();
}

EncodedNetPacket EncodeJoinPending(const JoinPendingPacket& packet) {
    PacketWriter writer(NetPacketType::JoinPending);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU8(static_cast<std::uint8_t>(packet.reason));
    writer.WriteU32(packet.pending_join_count);
    return writer.Finish();
}

EncodedNetPacket EncodePing(const PingPacket& packet) {
    PacketWriter writer(NetPacketType::Ping);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.sequence);
    writer.WriteU64(packet.sent_time_ms);
    return writer.Finish();
}

EncodedNetPacket EncodePong(const PongPacket& packet) {
    PacketWriter writer(NetPacketType::Pong);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.sequence);
    writer.WriteU64(packet.echoed_sent_time_ms);
    return writer.Finish();
}

EncodedNetPacket EncodeLeaveNotice(const LeaveNoticePacket& packet) {
    PacketWriter writer(NetPacketType::LeaveNotice);
    writer.WriteU32(packet.player_count);
    WritePlayerIds(writer, packet.player_ids);
    return writer.Finish();
}

EncodedNetPacket EncodeInputFrameRecords(const InputFrameRecordsPacket& packet) {
    PacketWriter writer(NetPacketType::InputFrameRecords);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.record_count);
    for (const InputFrameRecordEntry& record : packet.records) {
        writer.WriteU32(record.player_id);
        writer.WriteU64(record.frame);
        writer.WriteU32(record.sequence);
        writer.WriteU32(record.input_flags);
        writer.WriteU32(record.mouse_x);
        writer.WriteU32(record.mouse_y);
    }
    return writer.Finish();
}

EncodedNetPacket EncodeLockstepSettings(const LockstepSettingsPacket& packet) {
    PacketWriter writer(NetPacketType::LockstepSettings);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.sequence);
    writer.WriteU64(packet.apply_frame);
    writer.WriteU32(packet.input_delay_frames);
    writer.WriteU32(packet.max_rollback_frames);
    return writer.Finish();
}

EncodedNetPacket EncodeLockstepHash(const LockstepHashNetPacket& packet) {
    PacketWriter writer(NetPacketType::LockstepHash);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.sync_epoch);
    writer.WriteU64(packet.frame);
    writer.WriteU64(packet.hash);
    writer.WriteU64(packet.component_root);
    writer.WriteU64(packet.component_stage);
    writer.WriteU64(packet.component_players);
    writer.WriteU64(packet.component_tools);
    writer.WriteU64(packet.component_ents);
    return writer.Finish();
}

EncodedNetPacket EncodeSnapshotResyncRequest(const SnapshotResyncRequestPacket& packet) {
    PacketWriter writer(NetPacketType::SnapshotResyncRequest);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU64(packet.mismatch_frame);
    return writer.Finish();
}

EncodedNetPacket EncodeSnapshotResyncChunk(const SnapshotResyncChunkPacket& packet) {
    PacketWriter writer(NetPacketType::SnapshotResyncChunk);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.transfer_id);
    writer.WriteU32(packet.chunk_index);
    writer.WriteU32(packet.chunk_count);
    writer.WriteU32(packet.total_bytes);
    writer.WriteU32(packet.payload_bytes);
    writer.WriteU64(packet.snapshot_frame);
    writer.WriteBytes(packet.payload);
    return writer.Finish();
}

EncodedNetPacket EncodeSnapshotResyncAck(const SnapshotResyncAckPacket& packet) {
    PacketWriter writer(NetPacketType::SnapshotResyncAck);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.transfer_id);
    writer.WriteU64(packet.snapshot_frame);
    writer.WriteU8(packet.success);
    return writer.Finish();
}

EncodedNetPacket EncodeJoinBarrierStatus(const JoinBarrierStatusPacket& packet) {
    PacketWriter writer(NetPacketType::JoinBarrierStatus);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.barrier_id);
    writer.WriteU8(packet.active);
    writer.WriteU8(packet.phase);
    writer.WriteU32(packet.active_player_id);
    writer.WriteU32(packet.queued_peer_count);
    WritePlayerIds(writer, packet.queued_peer_ids);
    writer.WriteU32(packet.transfer_id);
    writer.WriteU64(packet.snapshot_frame);
    writer.WriteU32(packet.chunk_count);
    writer.WriteU32(packet.chunks_done);
    writer.WriteU32(packet.total_bytes);
    writer.WriteU32(packet.bytes_done);
    return writer.Finish();
}

EncodedNetPacket EncodeJoinBarrierResume(const JoinBarrierResumePacket& packet) {
    PacketWriter writer(NetPacketType::JoinBarrierResume);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.barrier_id);
    writer.WriteU64(packet.resume_frame);
    return writer.Finish();
}

EncodedNetPacket EncodeJoinBarrierTopology(const JoinBarrierTopologyPacket& packet) {
    PacketWriter writer(NetPacketType::JoinBarrierTopology);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.barrier_id);
    writer.WriteU64(packet.barrier_frame);
    writer.WriteU32(packet.player_count);
    WritePlayerIds(writer, packet.player_ids);
    WriteFloats(writer, packet.player_pos_x);
    WriteFloats(writer, packet.player_pos_y);
    writer.WriteU32(packet.removed_player_count);
    WritePlayerIds(writer, packet.removed_player_ids);
    return writer.Finish();
}

EncodedNetPacket EncodeJoinBarrierTopologyAck(const JoinBarrierTopologyAckPacket& packet) {
    PacketWriter writer(NetPacketType::JoinBarrierTopologyAck);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.barrier_id);
    writer.WriteU8(packet.success);
    return writer.Finish();
}

EncodedNetPacket EncodeRunRestart(const RunRestartPacket& packet) {
    PacketWriter writer(NetPacketType::RunRestart);
    writer.WriteU64(packet.stage_instance_id);
    writer.WriteU32(packet.sender_peer_id);
    writer.WriteU32(packet.sequence);
    writer.WriteU64(packet.apply_frame);
    writer.WriteU32(packet.stage_seed);
    writer.WriteChars(packet.quest_id);
    writer.WriteChars(packet.quest_stage_id);
    return writer.Finish();
}

std::optional<JoinRequestPacket> TryDecodeJoinRequest(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::JoinRequest);
    JoinRequestPacket packet;
    packet.local_player_count = reader.ReadU32();
    packet.preferred_player_count = reader.ReadU32();
    ReadPlayerIds(reader, packet.preferred_player_ids);
    reader.ReadChars(packet.display_name);
    if (!reader.Done()) {
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
    PacketReader reader(bytes, size, NetPacketType::JoinAccept);
    JoinAcceptPacket packet;
    packet.assigned_player_count = reader.ReadU32();
    ReadPlayerIds(reader, packet.assigned_player_ids);
    packet.host_player_id = reader.ReadU32();
    packet.stage_instance_id = reader.ReadU64();
    packet.remote_spawn_x = reader.ReadF32();
    packet.remote_spawn_y = reader.ReadF32();
    packet.host_spawn_x = reader.ReadF32();
    packet.host_spawn_y = reader.ReadF32();
    packet.stage_seed = reader.ReadU32();
    packet.lockstep_start_frame = reader.ReadU64();
    packet.lockstep_input_delay_frames = reader.ReadU32();
    packet.lockstep_max_rollback_frames = reader.ReadU32();
    packet.multiplayer_respawn_mode = reader.ReadU8();
    reader.ReadChars(packet.quest_id);
    reader.ReadChars(packet.quest_stage_id);
    reader.ReadChars(packet.host_name);
    if (!reader.Done()) {
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

std::optional<JoinPendingPacket> TryDecodeJoinPending(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::JoinPending);
    JoinPendingPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.reason = static_cast<JoinPendingReason>(reader.ReadU8());
    packet.pending_join_count = reader.ReadU32();
    if (!reader.Done()) {
        return std::nullopt;
    }
    if (packet.reason != JoinPendingReason::StageTransition) {
        packet.reason = JoinPendingReason::None;
    }
    return packet;
}

std::optional<PingPacket> TryDecodePing(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::Ping);
    PingPacket packet;
    packet.sender_peer_id = reader.ReadU32();
    packet.sequence = reader.ReadU32();
    packet.sent_time_ms = reader.ReadU64();
    if (!reader.Done()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<PongPacket> TryDecodePong(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::Pong);
    PongPacket packet;
    packet.sender_peer_id = reader.ReadU32();
    packet.sequence = reader.ReadU32();
    packet.echoed_sent_time_ms = reader.ReadU64();
    if (!reader.Done()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<LeaveNoticePacket> TryDecodeLeaveNotice(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::LeaveNotice);
    LeaveNoticePacket packet;
    packet.player_count = reader.ReadU32();
    ReadPlayerIds(reader, packet.player_ids);
    if (!reader.Done()) {
        return std::nullopt;
    }
    packet.player_count = std::min<std::uint32_t>(
        packet.player_count,
        static_cast<std::uint32_t>(packet.player_ids.size())
    );
    return packet;
}

std::optional<InputFrameRecordsPacket> TryDecodeInputFrameRecords(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::InputFrameRecords);
    InputFrameRecordsPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.record_count = reader.ReadU32();
    for (InputFrameRecordEntry& record : packet.records) {
        record.player_id = reader.ReadU32();
        record.frame = reader.ReadU64();
        record.sequence = reader.ReadU32();
        record.input_flags = reader.ReadU32();
        record.mouse_x = reader.ReadU32();
        record.mouse_y = reader.ReadU32();
    }
    if (!reader.Done()) {
        return std::nullopt;
    }
    packet.record_count = std::min<std::uint32_t>(
        packet.record_count,
        static_cast<std::uint32_t>(packet.records.size())
    );
    return packet;
}

std::optional<LockstepSettingsPacket> TryDecodeLockstepSettings(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::LockstepSettings);
    LockstepSettingsPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.sequence = reader.ReadU32();
    packet.apply_frame = reader.ReadU64();
    packet.input_delay_frames = reader.ReadU32();
    packet.max_rollback_frames = reader.ReadU32();
    if (!reader.Done()) {
        return std::nullopt;
    }
    packet.input_delay_frames = ClampLockstepInputDelayFrames(packet.input_delay_frames);
    packet.max_rollback_frames = ClampLockstepMaxRollbackFrames(packet.max_rollback_frames);
    return packet;
}

std::optional<LockstepHashNetPacket> TryDecodeLockstepHash(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::LockstepHash);
    LockstepHashNetPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.sync_epoch = reader.ReadU32();
    packet.frame = reader.ReadU64();
    packet.hash = reader.ReadU64();
    packet.component_root = reader.ReadU64();
    packet.component_stage = reader.ReadU64();
    packet.component_players = reader.ReadU64();
    packet.component_tools = reader.ReadU64();
    packet.component_ents = reader.ReadU64();
    if (!reader.Done()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<SnapshotResyncRequestPacket> TryDecodeSnapshotResyncRequest(
    const std::uint8_t* bytes,
    std::size_t size
) {
    PacketReader reader(bytes, size, NetPacketType::SnapshotResyncRequest);
    SnapshotResyncRequestPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.mismatch_frame = reader.ReadU64();
    if (!reader.Done()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<SnapshotResyncChunkPacket> TryDecodeSnapshotResyncChunk(
    const std::uint8_t* bytes,
    std::size_t size
) {
    PacketReader reader(bytes, size, NetPacketType::SnapshotResyncChunk);
    SnapshotResyncChunkPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.transfer_id = reader.ReadU32();
    packet.chunk_index = reader.ReadU32();
    packet.chunk_count = reader.ReadU32();
    packet.total_bytes = reader.ReadU32();
    packet.payload_bytes = reader.ReadU32();
    packet.snapshot_frame = reader.ReadU64();
    reader.ReadBytes(packet.payload);
    if (!reader.Done()) {
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
    PacketReader reader(bytes, size, NetPacketType::SnapshotResyncAck);
    SnapshotResyncAckPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.transfer_id = reader.ReadU32();
    packet.snapshot_frame = reader.ReadU64();
    packet.success = reader.ReadU8();
    if (!reader.Done()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<JoinBarrierStatusPacket> TryDecodeJoinBarrierStatus(
    const std::uint8_t* bytes,
    std::size_t size
) {
    PacketReader reader(bytes, size, NetPacketType::JoinBarrierStatus);
    JoinBarrierStatusPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.barrier_id = reader.ReadU32();
    packet.active = reader.ReadU8();
    packet.phase = reader.ReadU8();
    packet.active_player_id = reader.ReadU32();
    packet.queued_peer_count = reader.ReadU32();
    ReadPlayerIds(reader, packet.queued_peer_ids);
    packet.transfer_id = reader.ReadU32();
    packet.snapshot_frame = reader.ReadU64();
    packet.chunk_count = reader.ReadU32();
    packet.chunks_done = reader.ReadU32();
    packet.total_bytes = reader.ReadU32();
    packet.bytes_done = reader.ReadU32();
    if (!reader.Done()) {
        return std::nullopt;
    }
    packet.queued_peer_count = std::min<std::uint32_t>(
        packet.queued_peer_count,
        static_cast<std::uint32_t>(packet.queued_peer_ids.size())
    );
    packet.chunks_done = std::min(packet.chunks_done, packet.chunk_count);
    packet.bytes_done = std::min(packet.bytes_done, packet.total_bytes);
    return packet;
}

std::optional<JoinBarrierResumePacket> TryDecodeJoinBarrierResume(
    const std::uint8_t* bytes,
    std::size_t size
) {
    PacketReader reader(bytes, size, NetPacketType::JoinBarrierResume);
    JoinBarrierResumePacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.barrier_id = reader.ReadU32();
    packet.resume_frame = reader.ReadU64();
    if (!reader.Done()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<JoinBarrierTopologyPacket> TryDecodeJoinBarrierTopology(
    const std::uint8_t* bytes,
    std::size_t size
) {
    PacketReader reader(bytes, size, NetPacketType::JoinBarrierTopology);
    JoinBarrierTopologyPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.barrier_id = reader.ReadU32();
    packet.barrier_frame = reader.ReadU64();
    packet.player_count = reader.ReadU32();
    ReadPlayerIds(reader, packet.player_ids);
    ReadFloats(reader, packet.player_pos_x);
    ReadFloats(reader, packet.player_pos_y);
    packet.removed_player_count = reader.ReadU32();
    ReadPlayerIds(reader, packet.removed_player_ids);
    if (!reader.Done()) {
        return std::nullopt;
    }
    packet.player_count = std::min<std::uint32_t>(
        packet.player_count,
        static_cast<std::uint32_t>(packet.player_ids.size())
    );
    packet.removed_player_count = std::min<std::uint32_t>(
        packet.removed_player_count,
        static_cast<std::uint32_t>(packet.removed_player_ids.size())
    );
    return packet;
}

std::optional<JoinBarrierTopologyAckPacket> TryDecodeJoinBarrierTopologyAck(
    const std::uint8_t* bytes,
    std::size_t size
) {
    PacketReader reader(bytes, size, NetPacketType::JoinBarrierTopologyAck);
    JoinBarrierTopologyAckPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.barrier_id = reader.ReadU32();
    packet.success = reader.ReadU8();
    if (!reader.Done()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<RunRestartPacket> TryDecodeRunRestart(const std::uint8_t* bytes, std::size_t size) {
    PacketReader reader(bytes, size, NetPacketType::RunRestart);
    RunRestartPacket packet;
    packet.stage_instance_id = reader.ReadU64();
    packet.sender_peer_id = reader.ReadU32();
    packet.sequence = reader.ReadU32();
    packet.apply_frame = reader.ReadU64();
    packet.stage_seed = reader.ReadU32();
    reader.ReadChars(packet.quest_id);
    reader.ReadChars(packet.quest_stage_id);
    if (!reader.Done()) {
        return std::nullopt;
    }
    if (ReadFixedString(packet.quest_id).empty() ||
        ReadFixedString(packet.quest_stage_id).empty()) {
        return std::nullopt;
    }
    if (packet.stage_seed == 0) {
        packet.stage_seed = 1;
    }
    return packet;
}

} // namespace splonks::network

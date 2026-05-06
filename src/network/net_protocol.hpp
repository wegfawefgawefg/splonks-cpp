#pragma once

#include "entity/core_types.hpp"
#include "network/net_ids.hpp"
#include "frame_data_id.hpp"
#include "tile.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace splonks::network {

constexpr std::uint32_t kNetProtocolMagic = 0x534C504B; // SLPK
constexpr std::uint16_t kNetProtocolVersion = 1;
constexpr std::size_t kNetPacketMaxBytes = 512;
constexpr std::size_t kNetNameBytes = 32;
constexpr std::size_t kNetQuestIdBytes = 32;
constexpr std::size_t kNetQuestStageIdBytes = 64;
constexpr std::size_t kNetPlayersPerProcess = 16;
constexpr std::size_t kNetPlayerSnapshotsPerPacket = 8;
constexpr std::size_t kNetTileEventsPerPacket = 8;
constexpr std::size_t kNetEntitySpawnedEventsPerPacket = 5;
constexpr std::size_t kNetEntityDamageEventsPerPacket = 4;
constexpr std::size_t kNetEntityStateEventsPerPacket = 4;
constexpr std::size_t kNetEntityCarryEventsPerPacket = 5;
constexpr std::size_t kNetPresentationCommandEventsPerPacket = 4;

enum class NetPacketType : std::uint16_t {
    JoinRequest = 1,
    JoinAccept = 2,
    PlayerSnapshots = 3,
    TileEvents = 4,
    EntitySpawnedEvents = 5,
    EntityDamageEvents = 6,
    EntityStateEvents = 7,
    EntityCarryEvents = 8,
    LeaveNotice = 9,
    StageSync = 10,
    StageExitRequest = 11,
    PresentationCommandEvents = 12,
    DurableEventAck = 13,
};

struct NetPacketHeader {
    std::uint32_t magic = kNetProtocolMagic;
    std::uint16_t version = kNetProtocolVersion;
    NetPacketType type = NetPacketType::JoinRequest;
    std::uint16_t payload_bytes = 0;
};

struct JoinRequestPacket {
    std::uint32_t local_player_count = 1;
    std::array<char, kNetNameBytes> display_name{};
};

struct JoinAcceptPacket {
    std::uint32_t assigned_player_count = 1;
    std::array<PlayerId, kNetPlayersPerProcess> assigned_player_ids{};
    PlayerId coordinator_player_id = kPrimaryLocalPlayerId;
    StageInstanceId stage_instance_id = 1;
    float remote_spawn_x = 0.0F;
    float remote_spawn_y = 0.0F;
    float host_spawn_x = 0.0F;
    float host_spawn_y = 0.0F;
    std::uint32_t stage_seed = 1;
    std::array<char, kNetQuestIdBytes> quest_id{};
    std::array<char, kNetQuestStageIdBytes> quest_stage_id{};
    std::array<char, kNetNameBytes> coordinator_name{};
};

struct PlayerSnapshotEntry {
    PlayerId player_id = kInvalidPlayerId;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    std::uint8_t facing = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    std::uint16_t animation_flags = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct PlayerSnapshotsPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t sequence = 0;
    std::uint32_t snapshot_count = 0;
    std::array<PlayerSnapshotEntry, kNetPlayerSnapshotsPerPacket> snapshots{};
};

struct LeaveNoticePacket {
    std::uint32_t player_count = 0;
    std::array<PlayerId, kNetPlayersPerProcess> player_ids{};
};

struct StageSyncPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t stage_seed = 1;
    std::array<char, kNetQuestIdBytes> quest_id{};
    std::array<char, kNetQuestStageIdBytes> quest_stage_id{};
};

struct StageExitRequestPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    PlayerId player_id = kInvalidPlayerId;
    std::int32_t exit_id = -1;
};

struct DurableEventAckPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    PlayerId player_id = kInvalidPlayerId;
    std::uint64_t highest_applied_coordinator_order = 0;
};

struct TileEventEntry {
    NetEventId event_id = kInvalidNetEventId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t event_type = 0;
    std::uint16_t tile = 0;
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
};

struct TileEventsPacket {
    std::uint32_t event_count = 0;
    std::array<TileEventEntry, kNetTileEventsPerPacket> events{};
};

struct EntitySpawnedEventEntry {
    NetEventId event_id = kInvalidNetEventId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId held_by_id = kInvalidNetEntityId;
    std::uint32_t entity_type = 0;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    std::uint8_t use_pressed = 0;
    std::uint8_t reserved_a = 0;
    std::uint16_t reserved_b = 0;
};

struct EntitySpawnedEventsPacket {
    std::uint32_t event_count = 0;
    std::array<EntitySpawnedEventEntry, kNetEntitySpawnedEventsPerPacket> events{};
};

struct EntityDamageEventEntry {
    NetEventId event_id = kInvalidNetEventId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    std::uint32_t amount = 0;
    std::uint32_t remaining_health = 0;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    float acc_x = 0.0F;
    float acc_y = 0.0F;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    std::uint16_t damage_type = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct EntityDamageEventsPacket {
    std::uint32_t event_count = 0;
    std::array<EntityDamageEventEntry, kNetEntityDamageEventsPerPacket> events{};
};

struct EntityStateEventEntry {
    NetEventId event_id = kInvalidNetEventId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId entity_a_id = kInvalidNetEntityId;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    float acc_x = 0.0F;
    float acc_y = 0.0F;
    std::int32_t point_a_x = 0;
    std::int32_t point_a_y = 0;
    std::uint32_t health = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    float rotation = 0.0F;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t active = 0;
    std::uint8_t has_physics = 1;
    std::uint8_t can_collide = 1;
    std::uint8_t can_apply_projectile_contact = 1;
    std::uint8_t facing = 0;
};

struct EntityStateEventsPacket {
    std::uint32_t event_count = 0;
    std::array<EntityStateEventEntry, kNetEntityStateEventsPerPacket> events{};
};

struct EntityCarryEventEntry {
    NetEventId event_id = kInvalidNetEventId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t event_type = 0;
    std::uint16_t reserved = 0;
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId holder_id = kInvalidNetEntityId;
    NetEntityId thrower_id = kInvalidNetEntityId;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
};

struct EntityCarryEventsPacket {
    std::uint32_t event_count = 0;
    std::array<EntityCarryEventEntry, kNetEntityCarryEventsPerPacket> events{};
};

struct PresentationCommandEventEntry {
    NetEventId event_id = kInvalidNetEventId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t kind = 0;
    std::uint16_t effect_id = 0;
    std::uint32_t audio_asset_id = 0;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId target_entity_id = kInvalidNetEntityId;
    float source_x = 0.0F;
    float source_y = 0.0F;
    float target_x = 0.0F;
    float target_y = 0.0F;
    std::int32_t direction_x = 1;
    std::int32_t direction_y = 0;
    float param_a = 0.0F;
    float param_b = 0.0F;
    float param_c = 0.0F;
    float param_d = 0.0F;
};

struct PresentationCommandEventsPacket {
    std::uint32_t event_count = 0;
    std::array<PresentationCommandEventEntry, kNetPresentationCommandEventsPerPacket> events{};
};

struct EncodedNetPacket {
    std::array<std::uint8_t, kNetPacketMaxBytes> bytes{};
    std::size_t size = 0;
};

EncodedNetPacket EncodeJoinRequest(const JoinRequestPacket& packet);
EncodedNetPacket EncodeJoinAccept(const JoinAcceptPacket& packet);
EncodedNetPacket EncodePlayerSnapshots(const PlayerSnapshotsPacket& packet);
EncodedNetPacket EncodeTileEvents(const TileEventsPacket& packet);
EncodedNetPacket EncodeEntitySpawnedEvents(const EntitySpawnedEventsPacket& packet);
EncodedNetPacket EncodeEntityDamageEvents(const EntityDamageEventsPacket& packet);
EncodedNetPacket EncodeEntityStateEvents(const EntityStateEventsPacket& packet);
EncodedNetPacket EncodeEntityCarryEvents(const EntityCarryEventsPacket& packet);
EncodedNetPacket EncodePresentationCommandEvents(const PresentationCommandEventsPacket& packet);
EncodedNetPacket EncodeLeaveNotice(const LeaveNoticePacket& packet);
EncodedNetPacket EncodeStageSync(const StageSyncPacket& packet);
EncodedNetPacket EncodeStageExitRequest(const StageExitRequestPacket& packet);
EncodedNetPacket EncodeDurableEventAck(const DurableEventAckPacket& packet);
std::optional<JoinRequestPacket> TryDecodeJoinRequest(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinAcceptPacket> TryDecodeJoinAccept(const std::uint8_t* bytes, std::size_t size);
std::optional<PlayerSnapshotsPacket> TryDecodePlayerSnapshots(const std::uint8_t* bytes, std::size_t size);
std::optional<TileEventsPacket> TryDecodeTileEvents(const std::uint8_t* bytes, std::size_t size);
std::optional<EntitySpawnedEventsPacket> TryDecodeEntitySpawnedEvents(const std::uint8_t* bytes, std::size_t size);
std::optional<EntityDamageEventsPacket> TryDecodeEntityDamageEvents(const std::uint8_t* bytes, std::size_t size);
std::optional<EntityStateEventsPacket> TryDecodeEntityStateEvents(const std::uint8_t* bytes, std::size_t size);
std::optional<EntityCarryEventsPacket> TryDecodeEntityCarryEvents(const std::uint8_t* bytes, std::size_t size);
std::optional<PresentationCommandEventsPacket> TryDecodePresentationCommandEvents(const std::uint8_t* bytes, std::size_t size);
std::optional<LeaveNoticePacket> TryDecodeLeaveNotice(const std::uint8_t* bytes, std::size_t size);
std::optional<StageSyncPacket> TryDecodeStageSync(const std::uint8_t* bytes, std::size_t size);
std::optional<StageExitRequestPacket> TryDecodeStageExitRequest(const std::uint8_t* bytes, std::size_t size);
std::optional<DurableEventAckPacket> TryDecodeDurableEventAck(const std::uint8_t* bytes, std::size_t size);

template <std::size_t N>
std::string ReadFixedString(const std::array<char, N>& text) {
    std::size_t count = 0;
    while (count < text.size() && text[count] != '\0') {
        ++count;
    }
    return std::string(text.data(), count);
}

template <std::size_t N>
void WriteFixedString(const std::string& text, std::array<char, N>& out) {
    out.fill('\0');
    const std::size_t count = std::min(text.size(), out.size() - 1);
    std::memcpy(out.data(), text.data(), count);
}

} // namespace splonks::network

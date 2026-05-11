#pragma once

#include "effects/effect_id.hpp"
#include "entity/core_types.hpp"
#include "network/net_ids.hpp"
#include "network/net_limits.hpp"
#include "frame_data_id.hpp"
#include "tile.hpp"
#include "tools/tool_archetype.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace splonks::network {

constexpr std::uint32_t kNetProtocolMagic = 0x534C504B; // SLPK
constexpr std::uint16_t kNetProtocolVersion = 1;
constexpr std::size_t kNetNameBytes = 32;
constexpr std::size_t kNetQuestIdBytes = 32;
constexpr std::size_t kNetQuestStageIdBytes = 64;
constexpr std::size_t kNetPlayersPerProcess = 16;
constexpr std::size_t kNetPlayerSnapshotsPerPacket = 4;
constexpr std::size_t kNetTileMessagesPerPacket = 8;
constexpr std::size_t kNetFluidCellMessagesPerPacket = 4;
constexpr std::size_t kNetStageLightMessagesPerPacket = 4;
constexpr std::size_t kNetEntitySpawnedMessagesPerPacket = 1;
constexpr std::size_t kNetEntityDamageMessagesPerPacket = 3;
constexpr std::size_t kNetEntityStateMessagesPerPacket = 1;
constexpr std::size_t kNetEntityCarryMessagesPerPacket = 5;
constexpr std::size_t kNetEntityLifecycleMessagesPerPacket = 8;
constexpr std::size_t kNetPlayerStateMessagesPerPacket = 1;
constexpr std::size_t kNetRunStateMessagesPerPacket = 3;
constexpr std::size_t kNetPlayerStateToolSlotCount = 2;
constexpr std::size_t kNetPlayerStateEffectCount = 12;
constexpr std::size_t kNetEntityEffectCount = 12;
constexpr std::size_t kNetPresentationCommandMessagesPerPacket = 3;
constexpr std::size_t kNetActionRequestMessagesPerPacket = 4;
constexpr std::size_t kNetActionRequestAckMessageIdsPerPacket = 16;

enum class NetPacketType : std::uint16_t {
    JoinRequest = 1,
    JoinAccept = 2,
    PlayerSnapshots = 3,
    TileMessages = 4,
    EntitySpawnedMessages = 5,
    EntityDamageMessages = 6,
    EntityStateMessages = 7,
    EntityCarryMessages = 8,
    LeaveNotice = 9,
    StageSync = 10,
    PresentationCommandMessages = 11,
    DurableMessageAck = 12,
    ActionRequestMessages = 13,
    EntityLifecycleMessages = 14,
    PlayerStateMessages = 15,
    RunStateMessages = 16,
    ActionRequestAck = 17,
    FluidCellMessages = 18,
    StageLightMessages = 19,
};

struct NetPacketHeader {
    std::uint32_t magic = kNetProtocolMagic;
    std::uint16_t version = kNetProtocolVersion;
    NetPacketType type = NetPacketType::JoinRequest;
    std::uint16_t payload_bytes = 0;
};

struct JoinRequestPacket {
    std::uint32_t local_player_count = 1;
    std::uint32_t preferred_player_count = 0;
    std::array<PlayerId, kNetPlayersPerProcess> preferred_player_ids{};
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
    std::uint64_t snapshot_start_coordinator_order = 1;
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
    float acc_x = 0.0F;
    float acc_y = 0.0F;
    float size_x = 0.0F;
    float size_y = 0.0F;
    float rotation = 0.0F;
    std::uint32_t health = 0;
    std::uint32_t coyote_time = 0;
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    NetEntityId thrown_by_id = kInvalidNetEntityId;
    std::uint32_t movement_flags = 0;
    std::uint16_t projectile_contact_damage_amount = 0;
    std::uint16_t jump_hold_gravity_frames_remaining = 0;
    std::uint16_t jump_delay_frame_count = 0;
    std::uint16_t climb_detach_cooldown = 0;
    std::uint16_t hang_count = 0;
    std::uint16_t holding_timer = 0;
    std::uint16_t bomb_throw_delay_countdown = 0;
    std::uint16_t rope_throw_delay_countdown = 0;
    std::uint16_t attack_delay_countdown = 0;
    std::uint16_t equip_delay_countdown = 0;
    std::uint16_t thrown_immunity_timer = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
    std::uint16_t animation_frame = 0;
    std::uint8_t facing = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t has_physics = 1;
    std::uint8_t can_collide = 1;
    std::uint8_t can_apply_projectile_contact = 1;
    std::uint8_t projectile_contact_damage_type = 0;
    std::uint8_t hang_side = 0;
    std::uint8_t animate = 1;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    std::uint16_t input_flags = 0;
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
    std::uint64_t snapshot_start_coordinator_order = 0;
    std::array<char, kNetQuestIdBytes> quest_id{};
    std::array<char, kNetQuestStageIdBytes> quest_stage_id{};
    std::uint8_t force_resync = 0;
    std::array<std::uint8_t, 7> reserved{};
};

struct DurableMessageAckPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    PlayerId player_id = kInvalidPlayerId;
    std::uint64_t highest_applied_coordinator_order = 0;
};

struct TileMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t message_type = 0;
    std::uint16_t tile = 0;
    std::uint8_t rotation = 0;
    std::uint8_t layer = 0;
    std::uint16_t reserved = 0;
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
};

struct TileMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<TileMessageEntry, kNetTileMessagesPerPacket> messages{};
};

struct FluidCellMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t tile = 0;
    std::uint16_t reserved = 0;
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    float amount = 0.0F;
    float velocity_x = 0.0F;
    float velocity_y = 0.0F;
    float gravity_x = 0.0F;
    float gravity_y = 0.0F;
    float temp_gravity_x = 0.0F;
    float temp_gravity_y = 0.0F;
    float gravity_strength = 0.0F;
};

struct FluidCellMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<FluidCellMessageEntry, kNetFluidCellMessagesPerPacket> messages{};
};

struct StageLightMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t message_type = 0;
    std::uint16_t reserved = 0;
    std::uint64_t light_id = 0;
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    std::int32_t radius = 0;
};

struct StageLightMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<StageLightMessageEntry, kNetStageLightMessagesPerPacket> messages{};
};

struct EntityEffectEntry {
    std::uint16_t id = 0;
    std::uint16_t reserved = 0;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
};

struct EntitySpawnedMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
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
    float acc_x = 0.0F;
    float acc_y = 0.0F;
    float size_x = 0.0F;
    float size_y = 0.0F;
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float light_strength = 0.0F;
    float light_color_r = 1.0F;
    float light_color_g = 1.0F;
    float light_color_b = 1.0F;
    std::int32_t light_radius = 0;
    std::uint32_t movement_flags = 0;
    std::uint8_t effect_count = 0;
    std::array<EntityEffectEntry, kNetEntityEffectCount> effects{};
    std::uint8_t use_pressed = 0;
    std::uint8_t animate = 0;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct EntitySpawnedMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<EntitySpawnedMessageEntry, kNetEntitySpawnedMessagesPerPacket> messages{};
};

struct EntityDamageMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
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
    std::uint32_t fall_timer = 0;
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

struct EntityDamageMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<EntityDamageMessageEntry, kNetEntityDamageMessagesPerPacket> messages{};
};

struct EntityStateMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId entity_a_id = kInvalidNetEntityId;
    NetEntityId entity_b_id = kInvalidNetEntityId;
    NetEntityId entity_c_id = kInvalidNetEntityId;
    NetEntityId entity_d_id = kInvalidNetEntityId;
    NetEntityId holding_id = kInvalidNetEntityId;
    NetEntityId held_by_id = kInvalidNetEntityId;
    NetEntityId back_id = kInvalidNetEntityId;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    float acc_x = 0.0F;
    float acc_y = 0.0F;
    float size_x = 0.0F;
    float size_y = 0.0F;
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float counter_c = 0.0F;
    float counter_d = 0.0F;
    float threshold_a = 0.0F;
    float threshold_b = 0.0F;
    std::int32_t point_a_x = 0;
    std::int32_t point_a_y = 0;
    std::int32_t point_b_x = 0;
    std::int32_t point_b_y = 0;
    std::int32_t point_c_x = 0;
    std::int32_t point_c_y = 0;
    std::int32_t point_d_x = 0;
    std::int32_t point_d_y = 0;
    std::uint32_t health = 0;
    std::uint32_t coyote_time = 0;
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    float light_strength = 0.0F;
    float light_color_r = 1.0F;
    float light_color_g = 1.0F;
    float light_color_b = 1.0F;
    std::int32_t light_radius = 0;
    float rotation = 0.0F;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t active = 0;
    std::uint8_t has_physics = 1;
    std::uint8_t can_collide = 1;
    std::uint8_t can_apply_projectile_contact = 1;
    std::uint8_t damage_vulnerability = 0;
    std::uint8_t facing = 0;
    std::uint8_t ai_state = 0;
    std::uint8_t wanted = 0;
    std::uint8_t holding = 0;
    std::uint8_t render_enabled = 1;
    std::uint8_t attachment_mode = 0;
    std::uint8_t draw_layer = 0;
    std::uint32_t movement_flags = 0;
    std::uint32_t money = 0;
    std::int32_t stage_exit_id = -1;
    std::uint32_t runtime_flags = 0;
    std::uint8_t effect_count = 0;
    std::array<EntityEffectEntry, kNetEntityEffectCount> effects{};
    std::uint8_t buyable_active = 0;
    std::uint32_t buyable_display_quantity = 0;
    FrameDataId buyable_display_icon_animation_id = kInvalidFrameDataId;
    NetEntityId buyable_shop_owner_id = kInvalidNetEntityId;
    std::uint8_t animate = 0;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct EntityStateMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<EntityStateMessageEntry, kNetEntityStateMessagesPerPacket> messages{};
};

struct EntityCarryMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t message_type = 0;
    std::uint16_t attachment_mode = 0;
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId holder_id = kInvalidNetEntityId;
    NetEntityId thrower_id = kInvalidNetEntityId;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
};

struct EntityCarryMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<EntityCarryMessageEntry, kNetEntityCarryMessagesPerPacket> messages{};
};

struct EntityLifecycleMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t message_type = 0;
    std::uint16_t reserved = 0;
    NetEntityId entity_id = kInvalidNetEntityId;
};

struct EntityLifecycleMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<EntityLifecycleMessageEntry, kNetEntityLifecycleMessagesPerPacket> messages{};
};

struct PlayerStateToolSlotEntry {
    std::uint16_t kind = 0;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    std::uint8_t active = 0;
    std::uint8_t reserved = 0;
};

struct PlayerStateEffectEntry {
    std::uint16_t id = 0;
    std::uint16_t reserved = 0;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
};

struct PlayerStateMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    NetEntityId player_entity_id = kInvalidNetEntityId;
    PlayerId player_id = kInvalidPlayerId;
    std::uint32_t health = 0;
    std::uint32_t money = 0;
    std::uint8_t wanted = 0;
    std::uint8_t connected = 1;
    std::uint8_t effect_count = 0;
    std::uint8_t reserved = 0;
    std::array<PlayerStateToolSlotEntry, kNetPlayerStateToolSlotCount> tool_slots{};
    std::array<PlayerStateEffectEntry, kNetPlayerStateEffectCount> effects{};
};

struct PlayerStateMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<PlayerStateMessageEntry, kNetPlayerStateMessagesPerPacket> messages{};
};

struct RunStateMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
    std::uint16_t quest_id = 0;
    std::uint16_t stage_type = 0;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;
    std::uint32_t depth = 0;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::int32_t quest_level_number = 0;
    std::uint32_t generation_seed = 0;
    std::uint32_t tile_change_generation = 0;
    float stage_gravity = 0.0F;
    std::uint16_t border_left_tile = 0;
    std::uint16_t border_right_tile = 0;
    std::uint16_t border_top_tile = 0;
    std::uint16_t border_bottom_tile = 0;
    std::int32_t void_death_y = 0;
    std::uint32_t wrap_core_origin_x = 0;
    std::uint32_t wrap_core_origin_y = 0;
    std::uint32_t wrap_core_size_x = 0;
    std::uint32_t wrap_core_size_y = 0;
    std::uint8_t classic_made_black_market = 0;
    std::uint8_t classic_made_udjat_eye = 0;
    std::uint8_t classic_has_udjat_eye = 0;
    std::uint8_t classic_made_moai = 0;
    std::uint8_t classic_has_hedjet = 0;
    std::uint8_t classic_has_sceptre = 0;
    std::uint8_t classic_has_book_of_dead = 0;
    std::uint8_t has_generation_seed = 0;
    std::uint8_t border_wrap_x = 0;
    std::uint8_t border_wrap_y = 0;
    std::uint8_t has_void_death_y = 0;
    std::uint8_t camera_clamp_enabled = 1;
    std::uint8_t wrap_transform_active = 0;
    std::uint8_t game_over = 0;
    std::uint8_t win = 0;
    std::uint8_t has_snapshot_fingerprint = 0;
    std::uint16_t wrap_padding_tiles = 0;
    std::int32_t sac_altar_favor = 0;
    std::uint32_t sac_altar_reward_tier = 0;
    std::uint64_t snapshot_fingerprint = 0;
};

struct RunStateMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<RunStateMessageEntry, kNetRunStateMessagesPerPacket> messages{};
};

struct PresentationCommandMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
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
    std::uint32_t effect_count = 0;
    float effect_scale = 1.0F;
    float entity_shake_amount = 0.0F;
    float foreground_shake_amount = 0.0F;
    float background_shake_amount = 0.0F;
    float area_entity_shake_amount = 0.0F;
    float shake_radius_tiles = 0.0F;
    float light_strength = 0.0F;
    Color3 light_color = Color3::White();
    std::int32_t light_radius = 0;
    std::uint32_t light_lifetime_frames = 0;
};

struct PresentationCommandMessagesPacket {
    std::uint32_t message_count = 0;
    std::array<PresentationCommandMessageEntry, kNetPresentationCommandMessagesPerPacket> messages{};
};

struct ActionRequestMessageEntry {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint16_t action_kind = 0;
    std::uint16_t damage_type = 0;
    std::uint16_t projectile_contact_damage_type = 0;
    std::uint16_t flags = 0;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId target_entity_id = kInvalidNetEntityId;
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    std::int32_t direction_x = 0;
    std::int32_t direction_y = 0;
    float world_x = 0.0F;
    float world_y = 0.0F;
    float velocity_x = 0.0F;
    float velocity_y = 0.0F;
    std::uint32_t amount = 0;
    std::uint32_t projectile_contact_damage_amount = 0;
    std::uint32_t thrown_immunity_timer = 0;
    std::uint32_t projectile_contact_duration = 0;
    std::uint32_t tool_slot = 0;
    std::uint16_t use_edge = 0;
};

struct ActionRequestMessagesPacket {
    std::vector<ActionRequestMessageEntry> messages{};
};

struct ActionRequestAckPacket {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    PlayerId coordinator_player_id = kInvalidPlayerId;
    std::uint32_t ack_count = 0;
    std::array<NetMessageId, kNetActionRequestAckMessageIdsPerPacket> message_ids{};
};

struct EncodedNetPacket {
    std::array<std::uint8_t, kNetPacketMaxBytes> bytes{};
    std::size_t size = 0;
};

EncodedNetPacket EncodeJoinRequest(const JoinRequestPacket& packet);
EncodedNetPacket EncodeJoinAccept(const JoinAcceptPacket& packet);
EncodedNetPacket EncodePlayerSnapshots(const PlayerSnapshotsPacket& packet);
EncodedNetPacket EncodeTileMessages(const TileMessagesPacket& packet);
EncodedNetPacket EncodeFluidCellMessages(const FluidCellMessagesPacket& packet);
EncodedNetPacket EncodeStageLightMessages(const StageLightMessagesPacket& packet);
EncodedNetPacket EncodeEntitySpawnedMessages(const EntitySpawnedMessagesPacket& packet);
EncodedNetPacket EncodeEntityDamageMessages(const EntityDamageMessagesPacket& packet);
EncodedNetPacket EncodeEntityStateMessages(const EntityStateMessagesPacket& packet);
EncodedNetPacket EncodeEntityCarryMessages(const EntityCarryMessagesPacket& packet);
EncodedNetPacket EncodeEntityLifecycleMessages(const EntityLifecycleMessagesPacket& packet);
EncodedNetPacket EncodePlayerStateMessages(const PlayerStateMessagesPacket& packet);
EncodedNetPacket EncodeRunStateMessages(const RunStateMessagesPacket& packet);
EncodedNetPacket EncodePresentationCommandMessages(const PresentationCommandMessagesPacket& packet);
EncodedNetPacket EncodeActionRequestMessages(const ActionRequestMessagesPacket& packet);
EncodedNetPacket EncodeActionRequestAck(const ActionRequestAckPacket& packet);
EncodedNetPacket EncodeLeaveNotice(const LeaveNoticePacket& packet);
EncodedNetPacket EncodeStageSync(const StageSyncPacket& packet);
EncodedNetPacket EncodeDurableMessageAck(const DurableMessageAckPacket& packet);
std::optional<JoinRequestPacket> TryDecodeJoinRequest(const std::uint8_t* bytes, std::size_t size);
std::optional<JoinAcceptPacket> TryDecodeJoinAccept(const std::uint8_t* bytes, std::size_t size);
std::optional<PlayerSnapshotsPacket> TryDecodePlayerSnapshots(const std::uint8_t* bytes, std::size_t size);
std::optional<TileMessagesPacket> TryDecodeTileMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<FluidCellMessagesPacket> TryDecodeFluidCellMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<StageLightMessagesPacket> TryDecodeStageLightMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<EntitySpawnedMessagesPacket> TryDecodeEntitySpawnedMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<EntityDamageMessagesPacket> TryDecodeEntityDamageMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<EntityStateMessagesPacket> TryDecodeEntityStateMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<EntityCarryMessagesPacket> TryDecodeEntityCarryMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<EntityLifecycleMessagesPacket> TryDecodeEntityLifecycleMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<PlayerStateMessagesPacket> TryDecodePlayerStateMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<RunStateMessagesPacket> TryDecodeRunStateMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<PresentationCommandMessagesPacket> TryDecodePresentationCommandMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<ActionRequestMessagesPacket> TryDecodeActionRequestMessages(const std::uint8_t* bytes, std::size_t size);
std::optional<ActionRequestAckPacket> TryDecodeActionRequestAck(const std::uint8_t* bytes, std::size_t size);
std::optional<LeaveNoticePacket> TryDecodeLeaveNotice(const std::uint8_t* bytes, std::size_t size);
std::optional<StageSyncPacket> TryDecodeStageSync(const std::uint8_t* bytes, std::size_t size);
std::optional<DurableMessageAckPacket> TryDecodeDurableMessageAck(const std::uint8_t* bytes, std::size_t size);

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

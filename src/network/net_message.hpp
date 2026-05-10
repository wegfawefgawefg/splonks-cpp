#pragma once

#include "damage_types.hpp"
#include "entity/core_types.hpp"
#include "effects/effect_id.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "network/net_ids.hpp"
#include "quest.hpp"
#include "tile.hpp"
#include "tools/tool_archetype.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

namespace splonks::network {

constexpr std::size_t kPlayerStatePatchedToolSlotCount = 2;
constexpr std::size_t kPlayerStatePatchedEffectCount = 12;
constexpr std::size_t kEntityReplicatedEffectCount = 12;

enum class NetTileLayer : std::uint8_t {
    Foreground,
    Backwall,
};

enum class NetMessageType : std::uint16_t {
    None,

    PeerJoined,
    PeerLeft,
    PlayerSpawned,
    PlayerDespawned,
    StageLoaded,
    StageTransitionStarted,
    StageTransitionCommitted,
    RepairSnapshot,
    ActionRequest,

    EntitySpawned,
    EntityDeactivated,
    EntityStatePatched,
    EntityHeld,
    EntityDropped,
    EntityThrown,
    EntityDamaged,

    TileChanged,
    TileBroken,
    FluidCellPatched,

    PlayerStatePatched,
    RunStatePatched,
    PresentationCommand,
};

enum class NetActionKind : std::uint16_t {
    None,
    UseTool,
    PickupEntity,
    DropEntity,
    ThrowEntity,
    UseHeldEntity,
    UseBackEntity,
    PutHeldEntityOnBack,
    TakeOffBackEntity,
    InteractEntity,
    CollectEntity,
    PushEntity,
    BreakTile,
    DamageEntity,
    HitEntity,
};

enum class NetUseEdge : std::uint8_t {
    None,
    Press,
    Release,
};

constexpr std::uint16_t kActionRequestFlagClearVelocity = 1U << 0U;
constexpr std::uint16_t kActionRequestFlagClearAcceleration = 1U << 1U;
constexpr std::uint16_t kActionRequestFlagKnockbackOnNoDamage = 1U << 2U;

struct NetMessageHeader {
    NetMessageId message_id = kInvalidNetMessageId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
};

struct EntityReplicatedEffect {
    EffectId id = EffectId::None;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
};

struct EntitySpawnedMessage {
    NetEntityId entity_id = kInvalidNetEntityId;
    EntityType entity_type = EntityType::None;
    NetEntityId held_by_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    Vec2 size = Vec2::New(0.0F, 0.0F);
    NetEntityOwner owner = NetEntityOwner::Coordinator();
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    std::uint32_t movement_flags = 0;
    std::uint8_t effect_count = 0;
    std::array<EntityReplicatedEffect, kEntityReplicatedEffectCount> effects{};
    bool use_pressed = false;
    std::uint8_t animate = 0;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct EntityIdMessage {
    NetEntityId entity_id = kInvalidNetEntityId;
};

struct EntityHeldMessage {
    NetEntityId holder_id = kInvalidNetEntityId;
    NetEntityId held_id = kInvalidNetEntityId;
    AttachmentMode attachment_mode = AttachmentMode::Held;
};

struct EntityDroppedMessage {
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId dropped_by_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
};

struct EntityThrownMessage {
    NetEntityId entity_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    NetEntityId thrower_id = kInvalidNetEntityId;
};

struct EntityDamagedMessage {
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    std::uint32_t amount = 0;
    std::uint32_t remaining_health = 0;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
    DamageType damage_type = DamageType::Attack;
};

struct EntityStatePatchedMessage {
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId entity_a_id = kInvalidNetEntityId;
    NetEntityId entity_b_id = kInvalidNetEntityId;
    NetEntityId entity_c_id = kInvalidNetEntityId;
    NetEntityId entity_d_id = kInvalidNetEntityId;
    NetEntityId holding_id = kInvalidNetEntityId;
    NetEntityId held_by_id = kInvalidNetEntityId;
    NetEntityId back_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    Vec2 size = Vec2::New(0.0F, 0.0F);
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float counter_c = 0.0F;
    float counter_d = 0.0F;
    float threshold_a = 0.0F;
    float threshold_b = 0.0F;
    IVec2 point_a = IVec2::New(0, 0);
    IVec2 point_b = IVec2::New(0, 0);
    IVec2 point_c = IVec2::New(0, 0);
    IVec2 point_d = IVec2::New(0, 0);
    std::uint32_t health = 0;
    std::uint32_t coyote_time = 0;
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
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
    std::array<EntityReplicatedEffect, kEntityReplicatedEffectCount> effects{};
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

struct TileChangedMessage {
    IVec2 tile_pos = IVec2::New(0, 0);
    Tile tile = Tile::Air;
    TileRotation rotation = kTileRotation0;
    NetTileLayer layer = NetTileLayer::Foreground;
};

struct TileBrokenMessage {
    IVec2 tile_pos = IVec2::New(0, 0);
    NetEntityId source_entity_id = kInvalidNetEntityId;
};

struct FluidCellPatchedMessage {
    IVec2 tile_pos = IVec2::New(0, 0);
    Tile tile = Tile::Air;
    float amount = 0.0F;
    Vec2 velocity = Vec2::New(0.0F, 0.0F);
    Vec2 gravity = Vec2::New(0.0F, 0.0F);
    Vec2 temp_gravity = Vec2::New(0.0F, 0.0F);
    float gravity_strength = 0.0F;
};

struct PresentationCommandMessage {
    std::uint16_t kind = 0;
    std::uint16_t effect_id = 0;
    std::uint32_t audio_asset_id = 0;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId target_entity_id = kInvalidNetEntityId;
    Vec2 source_pos = Vec2::New(0.0F, 0.0F);
    Vec2 target_pos = Vec2::New(0.0F, 0.0F);
    std::int32_t direction_x = 1;
    std::int32_t direction_y = 0;
    float entity_shake_amount = 0.0F;
    float foreground_shake_amount = 0.0F;
    float background_shake_amount = 0.0F;
    float area_entity_shake_amount = 0.0F;
    float shake_radius_tiles = 0.0F;
};

struct PlayerStatePatchedToolSlot {
    ToolKind kind = ToolKind::ThrowPot;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    std::uint8_t active = 0;
};

struct PlayerStatePatchedEffect {
    EffectId id = EffectId::None;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
};

struct PlayerStatePatchedMessage {
    NetEntityId player_entity_id = kInvalidNetEntityId;
    PlayerId player_id = kInvalidPlayerId;
    std::uint32_t health = 0;
    std::uint32_t money = 0;
    std::uint8_t wanted = 0;
    std::uint8_t connected = 1;
    std::uint8_t effect_count = 0;
    std::array<PlayerStatePatchedToolSlot, kPlayerStatePatchedToolSlotCount> tool_slots{};
    std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount> effects{};
};

struct RunStatePatchedMessage {
    QuestId quest_id = QuestId::None;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;
    std::uint32_t depth = 0;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::uint32_t stage_type = 0;
    std::int32_t quest_level_number = 0;
    std::uint32_t generation_seed = 0;
    std::uint32_t tile_change_generation = 0;
    float stage_gravity = 0.3F;
    Tile border_left_tile = Tile::Air;
    Tile border_right_tile = Tile::Air;
    Tile border_top_tile = Tile::Air;
    Tile border_bottom_tile = Tile::Air;
    std::int32_t void_death_y = 0;
    std::uint8_t has_generation_seed = 0;
    std::uint8_t border_wrap_x = 0;
    std::uint8_t border_wrap_y = 0;
    std::uint8_t has_void_death_y = 0;
    std::uint8_t camera_clamp_enabled = 1;
    std::uint8_t wrap_transform_active = 0;
    std::uint8_t game_over = 0;
    std::uint8_t win = 0;
    std::uint16_t wrap_padding_tiles = 0;
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
    std::int32_t sac_altar_favor = 0;
    std::uint32_t sac_altar_reward_tier = 0;
    std::uint8_t has_snapshot_fingerprint = 0;
    std::uint64_t snapshot_fingerprint = 0;
};

struct StageLoadedMessage {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t seed = 0;
};

struct ActionRequestMessage {
    NetActionKind kind = NetActionKind::None;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId target_entity_id = kInvalidNetEntityId;
    IVec2 tile_pos = IVec2::New(0, 0);
    IVec2 direction = IVec2::New(0, 0);
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
    Vec2 velocity = Vec2::New(0.0F, 0.0F);
    DamageType damage_type = DamageType::Attack;
    DamageType projectile_contact_damage_type = DamageType::Attack;
    unsigned int amount = 0;
    unsigned int projectile_contact_damage_amount = 0;
    std::uint32_t thrown_immunity_timer = 0;
    std::uint32_t projectile_contact_duration = 0;
    bool clear_velocity = true;
    bool clear_acceleration = true;
    bool knockback_on_no_damage = false;
    std::uint32_t tool_slot = 0;
    NetUseEdge use_edge = NetUseEdge::None;
};

struct NetMessage {
    NetMessageHeader header{};
    NetMessageType type = NetMessageType::None;
    std::variant<
        std::monostate,
        EntitySpawnedMessage,
        EntityIdMessage,
        EntityHeldMessage,
        EntityDroppedMessage,
        EntityThrownMessage,
        EntityDamagedMessage,
        EntityStatePatchedMessage,
        TileChangedMessage,
        TileBrokenMessage,
        FluidCellPatchedMessage,
        PresentationCommandMessage,
        PlayerStatePatchedMessage,
        RunStatePatchedMessage,
        StageLoadedMessage,
        ActionRequestMessage
    > payload{};
};

} // namespace splonks::network

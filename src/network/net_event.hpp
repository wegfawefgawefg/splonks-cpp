#pragma once

#include "damage_types.hpp"
#include "entity/core_types.hpp"
#include "effects/effect_id.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "network/net_ids.hpp"
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

enum class NetEventType : std::uint16_t {
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
    RopeTilePlaced,

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
    PushEntity,
    BreakTile,
    DamageEntity,
    HitEntity,
};

struct NetEventHeader {
    NetEventId event_id = kInvalidNetEventId;
    PlayerId source_player_id = kInvalidPlayerId;
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint64_t source_local_frame = 0;
    std::uint64_t coordinator_order = 0;
};

struct EntitySpawnedEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
    EntityType entity_type = EntityType::None;
    NetEntityId held_by_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    NetEntityOwner owner = NetEntityOwner::Coordinator();
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    bool use_pressed = false;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct EntityIdEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
};

struct EntityHeldEvent {
    NetEntityId holder_id = kInvalidNetEntityId;
    NetEntityId held_id = kInvalidNetEntityId;
    AttachmentMode attachment_mode = AttachmentMode::Held;
};

struct EntityDroppedEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
};

struct EntityThrownEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    NetEntityId thrower_id = kInvalidNetEntityId;
};

struct EntityDamagedEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    std::uint32_t amount = 0;
    std::uint32_t remaining_health = 0;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
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

struct EntityStatePatchedEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId entity_a_id = kInvalidNetEntityId;
    NetEntityId holding_id = kInvalidNetEntityId;
    NetEntityId held_by_id = kInvalidNetEntityId;
    NetEntityId back_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    IVec2 point_a = IVec2::New(0, 0);
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
    std::uint8_t ai_state = 0;
    std::uint8_t wanted = 0;
    std::uint8_t attachment_mode = 0;
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

struct TileChangedEvent {
    IVec2 tile_pos = IVec2::New(0, 0);
    Tile tile = Tile::Air;
    TileRotation rotation = kTileRotation0;
};

struct TileBrokenEvent {
    IVec2 tile_pos = IVec2::New(0, 0);
    NetEntityId source_entity_id = kInvalidNetEntityId;
};

struct RopeTilePlacedEvent {
    IVec2 tile_pos = IVec2::New(0, 0);
    NetEntityId source_entity_id = kInvalidNetEntityId;
};

struct PresentationCommandEvent {
    std::uint16_t kind = 0;
    std::uint16_t effect_id = 0;
    std::uint32_t audio_asset_id = 0;
    NetEntityId source_entity_id = kInvalidNetEntityId;
    NetEntityId target_entity_id = kInvalidNetEntityId;
    Vec2 source_pos = Vec2::New(0.0F, 0.0F);
    Vec2 target_pos = Vec2::New(0.0F, 0.0F);
    std::int32_t direction_x = 1;
    std::int32_t direction_y = 0;
    float param_a = 0.0F;
    float param_b = 0.0F;
    float param_c = 0.0F;
    float param_d = 0.0F;
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

struct PlayerStatePatchedEvent {
    NetEntityId player_entity_id = kInvalidNetEntityId;
    std::uint32_t health = 0;
    std::uint32_t money = 0;
    std::uint8_t wanted = 0;
    std::uint8_t effect_count = 0;
    std::array<PlayerStatePatchedToolSlot, kPlayerStatePatchedToolSlotCount> tool_slots{};
    std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount> effects{};
};

struct RunStatePatchedEvent {
    std::int32_t sac_altar_favor = 0;
    std::uint32_t sac_altar_reward_tier = 0;
};

struct StageLoadedEvent {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t seed = 0;
};

struct ActionRequestEvent {
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
    std::uint32_t param_a = 0;
    std::uint32_t param_b = 0;
};

struct NetEvent {
    NetEventHeader header{};
    NetEventType type = NetEventType::None;
    std::variant<
        std::monostate,
        EntitySpawnedEvent,
        EntityIdEvent,
        EntityHeldEvent,
        EntityDroppedEvent,
        EntityThrownEvent,
        EntityDamagedEvent,
        EntityStatePatchedEvent,
        TileChangedEvent,
        TileBrokenEvent,
        RopeTilePlacedEvent,
        PresentationCommandEvent,
        PlayerStatePatchedEvent,
        RunStatePatchedEvent,
        StageLoadedEvent,
        ActionRequestEvent
    > payload{};
};

} // namespace splonks::network

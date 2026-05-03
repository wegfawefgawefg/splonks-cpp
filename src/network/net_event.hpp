#pragma once

#include "damage_types.hpp"
#include "entity/core_types.hpp"
#include "math_types.hpp"
#include "network/net_ids.hpp"
#include "tile.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace splonks::network {

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

    EntitySpawned,
    EntityDeactivated,
    EntityStatePatched,
    EntityOwnerChanged,
    EntityHeld,
    EntityDropped,
    EntityThrown,
    EntityDamaged,
    EntityKilled,
    EntityStunned,

    TileChanged,
    TileBroken,
    TileTriggerFired,
    RopeTilePlaced,
    FluidPatched,

    ToolSlotChanged,
    EffectAdded,
    EffectRemoved,
    MoneyChanged,
    FavorChanged,
    ShopDisturbed,
    ShopItemBought,
    QuestFlagChanged,

    BombPlaced,
    BombExploded,
    RopeThrown,
    ArrowFired,
    ProjectileHit,
    MattockDug,
    TeleporterUsed,
    CrateOpened,
    ChestOpened,
    SacrificeApplied,
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
    NetEntityOwner owner = NetEntityOwner::Coordinator();
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    bool use_pressed = false;
};

struct EntityIdEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
};

struct EntityOwnerChangedEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityOwner owner = NetEntityOwner::Coordinator();
};

struct EntityHeldEvent {
    NetEntityId holder_id = kInvalidNetEntityId;
    NetEntityId held_id = kInvalidNetEntityId;
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
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    DamageType damage_type = DamageType::Attack;
};

struct EntityStatePatchedEvent {
    NetEntityId entity_id = kInvalidNetEntityId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    std::uint32_t health = 0;
    std::uint32_t stun_timer = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t active = 0;
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

struct MoneyChangedEvent {
    PlayerId player_id = kInvalidPlayerId;
    std::int32_t delta = 0;
};

struct FavorChangedEvent {
    std::int32_t delta = 0;
    NetEntityId source_entity_id = kInvalidNetEntityId;
};

struct QuestFlagChangedEvent {
    std::uint32_t flag_id = 0;
    bool value = false;
};

struct StageLoadedEvent {
    StageInstanceId stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t seed = 0;
};

struct NetEvent {
    NetEventHeader header{};
    NetEventType type = NetEventType::None;
    std::variant<
        std::monostate,
        EntitySpawnedEvent,
        EntityIdEvent,
        EntityOwnerChangedEvent,
        EntityHeldEvent,
        EntityDroppedEvent,
        EntityThrownEvent,
        EntityDamagedEvent,
        EntityStatePatchedEvent,
        TileChangedEvent,
        TileBrokenEvent,
        RopeTilePlacedEvent,
        MoneyChangedEvent,
        FavorChangedEvent,
        QuestFlagChangedEvent,
        StageLoadedEvent
    > payload{};
};

} // namespace splonks::network

#pragma once

#include "damage_types.hpp"
#include "entity/core_types.hpp"
#include "math_types.hpp"
#include "player_id.hpp"
#include "presentation_commands.hpp"
#include "stage.hpp"
#include "stage_progression.hpp"
#include "vid.hpp"

#include <optional>
#include <cstdint>
#include <vector>

namespace splonks {

struct Audio;
struct Entity;
struct Graphics;
struct State;

enum class GameplayEventType {
    StageExitRequested,
    StageTransitionRequested,
    ActionRequested,
    EntitySpawned,
    EntityDeactivated,
    EntityHeld,
    EntityDropped,
    EntityThrown,
    EntityDamaged,
    EntityStatePatched,
    PlayerStatePatched,
    RunStatePatched,
    TileChanged,
    TileBroken,
    RopeTilePlaced,
    PresentationCommand,
};

enum class GameplayTileLayer : std::uint8_t {
    Foreground,
    Backwall,
};

enum class GameplayActionKind : std::uint16_t {
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

struct GameplayStageExitRequested {
    StageExitId exit_id = kInvalidStageExitId;
    PlayerId player_id = kInvalidPlayerId;
};

struct GameplayStageTransitionRequested {
    StageTransitionTarget target;
    PlayerId player_id = kInvalidPlayerId;
};

struct GameplayActionRequested {
    GameplayActionKind kind = GameplayActionKind::None;
    std::optional<VID> source_vid = std::nullopt;
    std::optional<VID> target_vid = std::nullopt;
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

struct GameplayEntitySpawned {
    VID entity_vid{};
    std::optional<VID> held_by_vid = std::nullopt;
    EntityType entity_type = EntityType::None;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    bool use_pressed = false;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct GameplayEntityDeactivated {
    VID entity_vid{};
};

struct GameplayEntityHeld {
    VID holder_vid{};
    VID held_vid{};
    AttachmentMode attachment_mode = AttachmentMode::Held;
};

struct GameplayEntityDropped {
    VID entity_vid{};
    std::optional<VID> dropped_by_vid = std::nullopt;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
};

struct GameplayEntityThrown {
    VID thrower_vid{};
    VID entity_vid{};
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
};

struct GameplayEntityDamaged {
    VID entity_vid{};
    std::optional<VID> source_vid = std::nullopt;
    DamageType damage_type = DamageType::Attack;
    unsigned int amount = 0;
    unsigned int remaining_health = 0;
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
};

struct GameplayEntityStatePatched {
    VID entity_vid{};
    VID source_vid{};
    std::optional<VID> entity_a_vid;
    std::optional<VID> entity_b_vid;
    std::optional<VID> entity_c_vid;
    std::optional<VID> entity_d_vid;
    std::optional<VID> holding_vid;
    std::optional<VID> held_by_vid;
    std::optional<VID> back_vid;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
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
    unsigned int health = 0;
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
    std::uint8_t draw_layer = 0;
    std::uint32_t runtime_flags = 0;
    std::uint8_t buyable_active = 0;
    std::uint32_t buyable_display_quantity = 0;
    FrameDataId buyable_display_icon_animation_id = kInvalidFrameDataId;
    std::optional<VID> buyable_shop_owner_vid;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct GameplayPlayerStatePatched {
    VID player_vid{};
};

struct GameplayRunStatePatched {
};

struct GameplayRopeTilePlaced {
    VID source_vid{};
    IVec2 tile_pos = IVec2::New(0, 0);
};

struct GameplayTileChanged {
    IVec2 tile_pos = IVec2::New(0, 0);
    Tile tile = Tile::Air;
    TileRotation rotation = kTileRotation0;
    GameplayTileLayer layer = GameplayTileLayer::Foreground;
};

struct GameplayTileBroken {
    IVec2 tile_pos = IVec2::New(0, 0);
};

struct GameplayEvent {
    GameplayEventType type = GameplayEventType::StageExitRequested;
    GameplayStageExitRequested stage_exit;
    GameplayStageTransitionRequested stage_transition;
    GameplayActionRequested action_requested;
    GameplayEntitySpawned entity_spawned;
    GameplayEntityDeactivated entity_deactivated;
    GameplayEntityHeld entity_held;
    GameplayEntityDropped entity_dropped;
    GameplayEntityThrown entity_thrown;
    GameplayEntityDamaged entity_damaged;
    GameplayEntityStatePatched entity_state_patched;
    GameplayPlayerStatePatched player_state_patched;
    GameplayRunStatePatched run_state_patched;
    GameplayTileChanged tile_changed;
    GameplayTileBroken tile_broken;
    GameplayRopeTilePlaced rope_tile_placed;
    PresentationCommand presentation_command;
};

void EmitStageExitRequested(State& state, StageExitId exit_id, PlayerId player_id);
void EmitStageTransitionRequested(State& state, const StageTransitionTarget& target, PlayerId player_id);
void EmitGameplayActionRequested(State& state, const GameplayActionRequested& request);
bool TryRequestOrApplyInteractEntity(
    VID source_vid,
    VID target_vid,
    State& state,
    Graphics& graphics,
    Audio& audio
);
void EmitEntitySpawnedGameplayEvent(
    State& state,
    const Entity& spawned_entity,
    std::optional<VID> held_by_vid = std::nullopt
);
void EmitEntityDeactivatedGameplayEvent(State& state, const Entity& entity);
void EmitEntityHeldGameplayEvent(
    State& state,
    const Entity& holder,
    const Entity& held,
    AttachmentMode attachment_mode = AttachmentMode::Held
);
void EmitEntityDroppedGameplayEvent(
    State& state,
    const Entity& entity,
    std::optional<VID> dropped_by_vid = std::nullopt
);
void EmitEntityThrownGameplayEvent(State& state, const Entity& thrower, const Entity& thrown, Vec2 throw_velocity);
void EmitEntityDamagedGameplayEvent(
    State& state,
    const Entity& entity,
    DamageType damage_type,
    unsigned int amount,
    std::optional<VID> source_vid = std::nullopt
);
void EmitEntityStatePatchedGameplayEvent(State& state, const Entity& source, const Entity& entity);
void EmitPlayerStatePatchedGameplayEvent(State& state, const Entity& player);
void EmitRunStatePatchedGameplayEvent(State& state);
void EmitTileChangedGameplayEvent(
    State& state,
    const IVec2& tile_pos,
    Tile tile,
    TileRotation rotation = kTileRotation0,
    GameplayTileLayer layer = GameplayTileLayer::Foreground
);
void EmitTileBrokenGameplayEvent(State& state, const IVec2& tile_pos);
void EmitRopeTilePlacedGameplayEvent(State& state, const Entity& source_entity, const IVec2& tile_pos);
void EmitPresentationCommandGameplayEvent(State& state, const PresentationCommand& command);
void ProcessGameplayEvents(State& state, Graphics& graphics, Audio& audio);

} // namespace splonks

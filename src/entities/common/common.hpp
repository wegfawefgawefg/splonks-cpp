#pragma once

#include "audio.hpp"
#include "entities/common/contact_types.hpp"
#include "entities/common/knockback.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "tools/tool_archetype.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace splonks::controls {
struct ControlIntent;
}

namespace splonks::entities::common {

constexpr float kMaxSpeed = 7.0F;
constexpr std::uint32_t kDefaultCoyoteTimeFrames = 6;
constexpr unsigned int kDefaultStunTimer = 60;
constexpr unsigned int kThrownByImmunityDuration = 16;
constexpr unsigned int kProjectileContactDuration = 120;

struct JumpAndClimbTuning {
    float gravity_scale = 1.0F;
    float jump_impulse = 4.5F;
    float spring_shoes_jump_impulse_bonus = 1.0F;
    float climb_speed = 3.0F;
    float climb_depart_horizontal_speed = 4.0F;
    float climb_probe_bias_pixels = 8.0F;
    float climb_probe_x_scale = 0.5F;
    std::uint32_t climb_required_probe_hits = 2;
    std::uint32_t coyote_time_frames = kDefaultCoyoteTimeFrames;
    std::uint32_t jump_delay_frames = 1;
    std::uint32_t jump_hold_gravity_frames = 0;
    std::uint32_t climb_detach_cooldown_frames = 5;
    std::uint32_t hang_drop_cooldown_frames = 5;
    std::uint32_t glove_hang_drop_cooldown_frames = 10;
    std::uint32_t hang_wall_release_cooldown_frames = 4;
    bool auto_ledge_grab = true;
};

struct TileContact {
    IVec2 tile_pos = IVec2::New(0, 0);
    const Tile* tile = nullptr;
    bool blocks_movement = false;
};

struct BlockingContactSet {
    bool touches_stage_bounds = false;
    std::vector<TileContact> tile_contacts;
    std::vector<VID> entity_vids;
};

void CommonStep(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);
void CommonPostStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void DieIfDead(std::size_t entity_idx, State& state, Audio& audio);
void OnDeathAsExplosion(std::size_t entity_idx, State& state, Audio& audio);
void ApplyDeactivateConditions(std::size_t entity_idx, State& state);
void StepStunTimer(std::size_t entity_idx, State& state);
void StepTravelSoundWalkerClimber(std::size_t entity_idx, State& state, Audio& audio);
void AccelerateHorizontallyTowardSpeed(Entity& entity, float target_speed, float max_acceleration);
void AccelerateHorizontallyTowardSpeed(
    Entity& entity,
    const State& state,
    float target_speed,
    float max_acceleration
);
void DecelerateHorizontallyToStop(Entity& entity, float max_acceleration, float snap_speed = 0.05F);
void StepAnimationTimer(std::size_t entity_idx, State& state, const Graphics& graphics, float dt);
void RefreshAllEntityFrameDataGeometry(State& state, const Graphics& graphics);
void EulerStep(std::size_t entity_idx, State& state, float dt);
void PrePartialEulerStep(std::size_t entity_idx, State& state, float dt);
void ApplyGravity(std::size_t entity_idx, State& state, float dt);
void ApplyEffectVelocityModifiers(Entity& entity, const State& state);
void ApplyGroundFriction(std::size_t entity_idx, State& state);
void ApplyGroundFriction(std::size_t entity_idx, State& state, float friction_scale);
void ApplyArchetypeGroundFriction(std::size_t entity_idx, State& state);
void ApplyArchetypeGroundFriction(std::size_t entity_idx, State& state, float friction_scale);
void StepStandardPhysics(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void PostPartialEulerStep(std::size_t entity_idx, State& state, float dt);
void GroundedCheck(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    bool check_tiles,
    bool check_entities,
    std::uint32_t coyote_time_frames = kDefaultCoyoteTimeFrames
);
bool IsGroundedOnTiles(std::size_t entity_idx, State& state);
void DoThrownByStep(std::size_t entity_idx, State& state);
void HangHandsStep(std::size_t entity_idx, State& state, const JumpAndClimbTuning& tuning);
void DoTileCollisions(std::size_t entity_idx, State& state);
void DoEntityCollisions(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
void DoTileAndEntityCollisions(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
void DoExplosion(
    std::size_t entity_idx,
    Vec2 center,
    float size,
    float push_magnitude,
    State& state,
    Audio& audio
);
const FrameData* GetCurrentFrameDataForEntity(const Entity& entity, const Graphics& graphics);
Vec2 GetSpriteTopLeftForEntity(const Entity& entity, const FrameData& frame_data);
Vec2 GetVisualCenterForEntity(const Entity& entity, const Graphics& graphics, const Vec2& fallback);
void SetVisualCenterForEntity(Entity& entity, const Graphics& graphics, const Vec2& center);
Vec2 GetEmitPointForEntity(const Entity& entity, const Graphics& graphics, const Vec2& fallback);
AABB GetContactAabbForEntity(const Entity& entity, const Graphics& graphics);
AABB GetEntityBroadphaseAabb(const Entity& entity, const Graphics& graphics);
bool CanCollectPickupFromContact(
    std::size_t pickup_idx,
    std::size_t collector_idx,
    const State& state
);
bool TryRequestCollectPickupFromContact(
    std::size_t pickup_idx,
    std::size_t collector_idx,
    State& state
);
void DeactivateCollectedPickup(std::size_t pickup_idx, State& state, const Graphics& graphics);
void CleanupInactiveCarryReferences(std::size_t entity_idx, State& state);
void AttachEntityAsHeld(Entity& holder, Entity& held);
void ReleaseEntityFromHolder(Entity& entity, State& state);
void ReleaseEntityFromHolderAndEmitNetwork(Entity& entity, State& state);
std::vector<VID> SeverEntityCarryLinksForReset(Entity& entity, State& state);
void DropHeldItemFromEntity(Entity& entity, State& state);
bool TryPickupEntityByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
);
bool TryDropEntityByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
);
bool TryThrowEntityByVid(
    VID thrower_vid,
    VID thrown_vid,
    Vec2 throw_velocity,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
bool TryPutHeldEntityOnBackByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
);
bool TryTakeOffBackEntityByVid(
    VID holder_vid,
    VID back_vid,
    State& state,
    const Graphics& graphics
);
void UpdateCarryAndBackItems(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void SyncEntityAttachments(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics
);
bool TryApplyStompContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void TryPushBlocks(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics
);
bool TryApplyPushEntityAction(
    VID pusher_vid,
    VID pushed_vid,
    float push_acc_delta,
    State& state,
    const Graphics& graphics
);
bool TryDisplaceEntityByOnePixel(
    std::size_t entity_idx,
    const IVec2& direction,
    State& state,
    const Graphics& graphics,
    Audio* audio
);
bool TryApplyCrusherPusherContact(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
bool TrySpawnAndThrowEntityForToolUse(
    std::size_t thrower_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    ToolSlot& tool_slot,
    bool trigger_pressed,
    std::uint16_t cooldown_frames,
    std::uint32_t thrown_immunity_timer,
    void (*setup_entity)(Entity&),
    ToolThrowVelocityBuilder build_throw_velocity = nullptr,
    std::optional<Vec2> throw_velocity_override = std::nullopt
);
bool TryUseToolSlot(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity = nullptr,
    std::optional<Vec2> throw_velocity_override = std::nullopt
);

enum class DamageResult {
    None,
    Requested,
    Hurt,
    Died,
};

struct DamageOptions {
    std::optional<VID> source_vid = std::nullopt;
    bool allow_remote_player_target = false;
    bool defer_replication = false;
};

struct HitOptions {
    std::optional<VID> source_vid = std::nullopt;
    KnockbackSpec knockback;
    bool allow_remote_player_target = true;
    bool knockback_on_no_damage = false;
};

bool CanEntityTakeDamageType(const Entity& entity, DamageType damage_type);
DamageResult TryDamageEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    DamageOptions options = {}
);
DamageResult TryHitEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    HitOptions options
);
bool TryApplyProjectileContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);

void JumpingAndClimbingStep(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    const JumpAndClimbTuning& tuning
);
bool TryApplySwimImpulse(Entity& entity, State& state, Audio& audio);
ContactResolution TryDispatchEntityEntityContactPair(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
);
ContactResolution TryDispatchEntityEntityContacts(
    std::size_t entity_idx,
    const std::vector<VID>& touched_vids,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
);
// Use this after an entity's contact shape was moved outside DoTileAndEntityCollisions().
// Typical use is manually positioned entities with has_physics == false, like held/swinging items.
// Do not call this for entities that still go through the normal physics collision path, even when
// their velocity is zero, because MoveEntityPixelStep already does a final overlap dispatch for them.
bool TryDispatchEntityEntityOverlapContacts(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio,
    const ContactContext& context
);
std::vector<VID> GatherTouchedEntityContactsForAabb(
    std::size_t entity_idx,
    const AABB& aabb,
    const Graphics& graphics,
    State& state
);
BlockingContactSet GatherBlockingContactsForAabb(
    std::size_t entity_idx,
    const AABB& aabb,
    const State& state,
    bool check_tiles,
    bool check_entities
);
ContactResolution ResolveBlockingContactSet(
    std::size_t entity_idx,
    const BlockingContactSet& contacts,
    const State& state
);
ContactResolution TryDispatchEntityTileContacts(
    std::size_t entity_idx,
    const BlockingContactSet& contacts,
    const ContactContext& context,
    State& state,
    Audio* audio
);

} // namespace splonks::entities::common

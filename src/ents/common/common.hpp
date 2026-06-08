#pragma once

#include "audio.hpp"
#include "ents/common/contact_types.hpp"
#include "ents/common/knockback.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "tools/tool_spec.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace splonks::controls {
struct ControlIntent;
}

namespace splonks::ents::common {

constexpr float kMaxSpeed = 7.0F;
constexpr std::uint32_t kDefaultCoyoteTimeFrames = 6;
constexpr std::uint32_t kDefaultStunTimer = 60;
constexpr std::uint32_t kThrownByImmunityDuration = 16;
constexpr std::uint32_t kProjContactDuration = 120;

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
    std::vector<VID> ent_vids;
};

void CommonStep(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio, float dt);
void CommonPostStep(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void DieIfDead(std::size_t ent_idx, State& state, Audio& audio);
void OnDeathAsExplosion(std::size_t ent_idx, State& state, Audio& audio);
void ApplyDeactivateConditions(std::size_t ent_idx, State& state);
void StepStunTimer(std::size_t ent_idx, State& state);
void StepTravelSoundWalkerClimber(std::size_t ent_idx, State& state, Audio& audio);
void AccelerateHorizontallyTowardSpeed(Ent& ent, float target_speed, float max_acceleration);
void AccelerateHorizontallyTowardSpeed(
    Ent& ent,
    const State& state,
    float target_speed,
    float max_acceleration
);
void DecelerateHorizontallyToStop(Ent& ent, float max_acceleration, float snap_speed = 0.05F);
void StepAnimTimer(std::size_t ent_idx, State& state, const Graphics& graphics, float dt);
void RefreshAllEntAFrameGeometry(State& state, const Graphics& graphics);
void EulerStep(std::size_t ent_idx, State& state, float dt);
void PrePartialEulerStep(std::size_t ent_idx, State& state, float dt);
void ApplyGravity(std::size_t ent_idx, State& state, float dt);
void ApplyEffectVelocityModifiers(Ent& ent, const State& state);
void ApplyGroundFriction(std::size_t ent_idx, State& state);
void ApplyGroundFriction(std::size_t ent_idx, State& state, float friction_scale);
void ApplySpecGroundFriction(std::size_t ent_idx, State& state);
void ApplySpecGroundFriction(std::size_t ent_idx, State& state, float friction_scale);
void StepStandardPhysics(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void PostPartialEulerStep(std::size_t ent_idx, State& state, float dt);
void GroundedCheck(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    bool check_tiles,
    bool check_ents,
    std::uint32_t coyote_time_frames = kDefaultCoyoteTimeFrames
);
bool IsGroundedOnTiles(std::size_t ent_idx, State& state);
void DoThrownByStep(std::size_t ent_idx, State& state);
void HangHandsStep(std::size_t ent_idx, State& state, const JumpAndClimbTuning& tuning);
void DoTileCollisions(std::size_t ent_idx, State& state);
void DoEntCollisions(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio);
void DoTileAndEntCollisions(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
void DoExplosion(
    std::size_t ent_idx,
    Vec2 center,
    float size,
    float push_magnitude,
    State& state,
    Audio& audio
);
bool TryApplyPlausibleLocomotionClaim(
    Ent& ent,
    State& state,
    const JumpAndClimbTuning& tuning,
    sim::Vec2 claimed_pos,
    sim::Vec2 claimed_vel,
    sim::Vec2 claimed_acc,
    std::uint32_t claimed_movement_flags,
    bool claimed_grounded,
    std::optional<Side> claimed_hang_side,
    std::uint32_t claimed_coyote_time,
    std::uint32_t claimed_fall_timer,
    std::uint32_t claimed_hang_count,
    std::uint32_t claimed_climb_detach_cooldown
);
const AFrame* GetCurrentAFrameForEnt(const Ent& ent, const Graphics& graphics);
Vec2 GetSpriteTopLeftForEnt(const Ent& ent, const AFrame& aframe);
sim::Vec2 GetSimSpriteTopLeftForEnt(const Ent& ent, const AFrame& aframe);
Vec2 GetVisualCenterForEnt(const Ent& ent, const Graphics& graphics, const Vec2& fallback);
void SetVisualCenterForEnt(Ent& ent, const Graphics& graphics, const Vec2& center);
Vec2 GetEmitPointForEnt(const Ent& ent, const Graphics& graphics, const Vec2& fallback);
AABB GetRenderContactAabbForEnt(const Ent& ent, const Graphics& graphics);
sim::AABB GetContactAabbForEnt(const Ent& ent, const Graphics& graphics);
AABB GetRenderEntBroadphaseAabb(const Ent& ent, const Graphics& graphics);
sim::AABB GetEntBroadphaseAabb(const Ent& ent, const Graphics& graphics);
bool CanCollectPickupFromContact(
    std::size_t pickup_idx,
    std::size_t collector_idx,
    const State& state
);
void DeactivateCollectedPickup(std::size_t pickup_idx, State& state, const Graphics& graphics);
void CleanupInactiveCarryReferences(std::size_t ent_idx, State& state);
void AttachEntAsHeld(Ent& holder, Ent& held);
void ReleaseEntFromHolder(Ent& ent, State& state);
void ReleaseEntFromHolderIfAttached(Ent& ent, State& state);
std::vector<VID> SeverEntCarryLinksForReset(Ent& ent, State& state);
std::vector<VID> SeverEntOutboundCarryLinksForReset(Ent& ent, State& state);
void DropHeldItemFromEnt(Ent& ent, State& state);
bool TryPickupEntByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
);
bool TryDropEntByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
);
bool TryThrowEntByVid(
    VID thrower_vid,
    VID thrown_vid,
    Vec2 throw_velocity,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
bool TryPutHeldEntOnBackByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
);
bool TryTakeOffBackEntByVid(
    VID holder_vid,
    VID back_vid,
    State& state,
    const Graphics& graphics
);
void UpdateCarryAndBackItems(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
void SyncEntAttachs(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics
);
bool TryApplyStompContactToEnt(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void TryPushBlocks(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics
);
bool TryApplyPushEntAction(
    VID pusher_vid,
    VID pushed_vid,
    float push_acc_delta,
    State& state,
    const Graphics& graphics
);
bool TryDisplaceEntByOnePixel(
    std::size_t ent_idx,
    const IVec2& direction,
    State& state,
    const Graphics& graphics,
    Audio* audio
);
bool TryApplyCrusherPusherContact(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
bool TrySpawnAndThrowEntForToolUse(
    std::size_t thrower_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    ToolSlot& tool_slot,
    bool trigger_pressed,
    std::uint16_t cooldown_frames,
    std::uint32_t thrown_immunity_timer,
    void (*setup_ent)(Ent&),
    ToolThrowVelocityBuilder build_throw_velocity = nullptr,
    std::optional<Vec2> throw_velocity_override = std::nullopt
);
bool TryUseToolSlot(
    std::size_t ent_idx,
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
    Hurt,
    Died,
};

struct DamageOptions {
    std::optional<VID> source_vid = std::nullopt;
};

struct HitOptions {
    std::optional<VID> source_vid = std::nullopt;
    KnockbackSpec knockback;
    bool knockback_on_no_damage = false;
};

bool CanEntTakeDamageType(const Ent& ent, DamageType damage_type);
DamageResult TryDamageEnt(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    DamageOptions options = {}
);
DamageResult TryHitEnt(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    HitOptions options
);
bool TryApplyProjContactToEnt(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);

void JumpingAndClimbingStep(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    const JumpAndClimbTuning& tuning
);
bool TryApplySwimImpulse(Ent& ent, State& state, Audio& audio);
ContactResult TryDispatchEntEntContactPair(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
);
ContactResult TryDispatchEntEntContacts(
    std::size_t ent_idx,
    const std::vector<VID>& touched_vids,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
);
// Use this after an ent's contact shape was moved outside DoTileAndEntCollisions().
// Typical use is manually positioned ents with has_physics == false, like held/swinging items.
// Do not call this for ents that still go through the normal physics collision path, even when
// their velocity is zero, because MoveEntPixelStep already does a final overlap dispatch for them.
bool TryDispatchEntEntOverlapContacts(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio,
    const ContactContext& context
);
std::vector<VID> GatherTouchedEntContactsForAabb(
    std::size_t ent_idx,
    sim::AABB aabb,
    const Graphics& graphics,
    State& state
);
BlockingContactSet GatherBlockingContactsForAabb(
    std::size_t ent_idx,
    sim::AABB aabb,
    const State& state,
    bool check_tiles,
    bool check_ents
);
ContactResult ResolveBlockingContactSet(
    std::size_t ent_idx,
    const BlockingContactSet& contacts,
    const State& state
);
ContactResult TryDispatchEntTileContacts(
    std::size_t ent_idx,
    const BlockingContactSet& contacts,
    const ContactContext& context,
    State& state,
    Audio* audio
);

} // namespace splonks::ents::common

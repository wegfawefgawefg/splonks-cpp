#pragma once

#include "audio.hpp"
#include "entities/common/knockback.hpp"
#include "entity.hpp"
#include "graphics.hpp"
#include "state.hpp"

namespace splonks::controls {
struct ControlIntent;
}

namespace splonks::entities::common {

constexpr float kMaxSpeed = 7.0F;
constexpr unsigned int kDefaultStunTimer = 60;
constexpr unsigned int kThrownByImmunityDuration = 16;
constexpr unsigned int kProjectileContactDuration = 120;

enum class BlockingImpactAxis {
    Horizontal,
    Vertical,
};

enum class BlockingImpactSurface {
    StageBounds,
    Tiles,
    ImpassableEntity,
};

enum class ContactPhase {
    SweptEntered,
    AttemptedBlocked,
};

struct ContactContext {
    ContactPhase phase = ContactPhase::SweptEntered;
    bool has_impact = false;
    BlockingImpactAxis impact_axis = BlockingImpactAxis::Horizontal;
    BlockingImpactSurface impact_surface = BlockingImpactSurface::Tiles;
    float impact_velocity = 0.0F;
    int direction = 0;
    std::optional<VID> mover_vid = std::nullopt;
    std::optional<VID> other_vid = std::nullopt;
};

struct ContactResolution {
    // The attempted pixel move may not be entered.
    bool blocks_movement = false;
    // Stop all further movement processing in the current MoveEntityPixelStep call.
    bool stop_sweep = false;
};

struct TileContact {
    IVec2 tile_pos = IVec2::New(0, 0);
    const Tile* tile = nullptr;
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
void StepAnimationTimer(std::size_t entity_idx, State& state, const Graphics& graphics, float dt);
void EulerStep(std::size_t entity_idx, State& state, float dt);
void PrePartialEulerStep(std::size_t entity_idx, State& state, float dt);
void ApplyGravity(std::size_t entity_idx, State& state, float dt);
void ApplyGroundFriction(std::size_t entity_idx, State& state);
void ApplyArchetypeGroundFriction(std::size_t entity_idx, State& state);
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
    bool check_entities
);
bool IsGroundedOnTiles(std::size_t entity_idx, State& state);
void DoThrownByStep(std::size_t entity_idx, State& state);
void HangHandsStep(std::size_t entity_idx, State& state);
void DoTileCollisions(std::size_t entity_idx, State& state);
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
    State& state,
    Audio& audio
);
const FrameData* GetCurrentFrameDataForEntity(const Entity& entity, const Graphics& graphics);
Vec2 GetSpriteTopLeftForEntity(const Entity& entity, const FrameData& frame_data);
Vec2 GetEmitPointForEntity(const Entity& entity, const Graphics& graphics, const Vec2& fallback);
AABB GetContactAabbForEntity(const Entity& entity, const Graphics& graphics);
AABB GetEntityBroadphaseAabb(const Entity& entity, const Graphics& graphics);
bool TryCollectEntityFromContact(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void CleanupInactiveCarryReferences(std::size_t entity_idx, State& state);
void ReleaseEntityFromHolder(Entity& entity, State& state);
void DropHeldItemFromEntity(Entity& entity, State& state);
void UpdateCarryAndBackItems(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
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
using ToolThrowVelocityBuilder = Vec2 (*)(const controls::ControlIntent&);
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
    ToolThrowVelocityBuilder build_throw_velocity = nullptr
);
bool TryUseToolSlot(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed
);

enum class DamageResult {
    None,
    Hurt,
    Died,
};

DamageResult TryDamageEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount
);
bool TryApplyProjectileContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);

void JumpingAndClimbingStep(std::size_t entity_idx, State& state, Audio& audio);
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

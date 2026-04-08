#pragma once

#include "audio.hpp"
#include "entity.hpp"
#include "graphics.hpp"
#include "state.hpp"

namespace splonks::entities::common {

constexpr float kMaxSpeed = 7.0F;
constexpr unsigned int kDefaultStunTimer = 60;
constexpr unsigned int kThrownByImmunityDuration = 16;

void CommonStep(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);
void CommonPostStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void ApplyDeactivateConditions(std::size_t entity_idx, State& state);
void StoreHealthToLastHealth(std::size_t entity_idx, State& state);
void StepStunTimer(std::size_t entity_idx, State& state);
void StepTravelSoundWalkerClimber(std::size_t entity_idx, State& state, Audio& audio);
void StepAnimationTimer(std::size_t entity_idx, State& state, const Graphics& graphics, float dt);
void EulerStep(std::size_t entity_idx, State& state, float dt);
void PrePartialEulerStep(std::size_t entity_idx, State& state, float dt);
void ApplyGravity(std::size_t entity_idx, State& state, float dt);
void ApplyGroundFriction(std::size_t entity_idx, State& state);
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
void DoExplosion(std::size_t entity_idx, Vec2 center, float size, State& state, Audio& audio);
const FrameData* GetCurrentFrameDataForEntity(const Entity& entity, const Graphics& graphics);
Vec2 GetSpriteTopLeftForEntity(const Entity& entity, const FrameData& frame_data);
AABB GetContactAabbForEntity(const Entity& entity, const Graphics& graphics);
void CollectTouchingPickups(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);

enum class DamageResult {
    None,
    Hurt,
    Died,
};

DamageResult TryToDamageEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount
);

void JumpingAndClimbingStep(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::common

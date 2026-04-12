#pragma once

#include "entity/archetype.hpp"

#include <cstddef>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::player {

constexpr IVec2 kSize = {10, 10};
constexpr float kMoveAcc = 0.5F;
constexpr float kRunAcc = 0.5F;
constexpr float kClimbSpeed = 3.0F;
constexpr float kMaxWalkSpeed = 2.5F;
constexpr float kMaxRunSpeed = 4.0F;
constexpr float kMaxSpeed = 9.0F;
constexpr float kJumpImpulse = 4.5F;
constexpr unsigned int kCoyoteTimeFrames = 6;
constexpr unsigned int kJumpDelayFrames = 1;
constexpr unsigned int kBombThrowDelay = 8;
constexpr unsigned int kRopeThrowDelay = 8;
constexpr unsigned int kPotThrowDelay = 12;
constexpr unsigned int kAttackDelay = 24;
constexpr unsigned int kEquipDelay = 8;

extern const EntityArchetype kPlayerArchetype;

void StepEntityLogicAsPlayer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntityPhysicsAsPlayer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void SyncEntitySpriteToDisplayStatePlayer(Entity& entity);

} // namespace splonks::entities::player

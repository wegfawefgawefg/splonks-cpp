#pragma once

#include "ent/spec.hpp"

#include <cstddef>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::player {

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

extern const EntSpec kPlayerSpec;

void ControlEntAsPlayer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntLogicAsPlayer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntPhysicsAsPlayer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void SyncEntSpriteToDisplayStatePlayer(Ent& ent);

} // namespace splonks::ents::player

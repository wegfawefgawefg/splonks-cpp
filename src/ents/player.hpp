#pragma once

#include "ent/spec.hpp"

#include <cstddef>
#include <cstdint>

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
constexpr std::uint32_t kCoyoteTimeFrames = 6;
constexpr std::uint32_t kJumpDelayFrames = 1;
constexpr std::uint32_t kBombThrowDelay = 8;
constexpr std::uint32_t kRopeThrowDelay = 8;
constexpr std::uint32_t kPotThrowDelay = 12;
constexpr std::uint32_t kAttackDelay = 24;
constexpr std::uint32_t kEquipDelay = 8;

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

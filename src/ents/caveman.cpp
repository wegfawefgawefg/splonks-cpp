#include "ents/caveman.hpp"
#include "audio.hpp"
#include "ents/common/common.hpp"
#include "ents/common/ground_walker.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>

namespace splonks::ents::caveman {

namespace {

constexpr float kCavemanWalkSpeed = 0.8F;
constexpr float kCavemanAttackSpeed = 1.6F;
constexpr float kCavemanWalkAcceleration = 0.18F;
constexpr float kCavemanAttackAcceleration = 0.24F;
constexpr float kCavemanWallHopSpeedX = 1.0F;
constexpr float kCavemanWallHopSpeedY = -1.0F;
constexpr float kCavemanAlertHopSpeedY = -1.0F;
constexpr int kCavemanSightVerticalTolerance = 12;
constexpr int kCavemanSightDistance = 100;
constexpr std::uint64_t kCavemanSightScanIntervalFrames = 30;
constexpr int kCavemanIdleMinFrames = 24;
constexpr int kCavemanIdleMaxFrames = 64;
constexpr int kCavemanIdleChance = 120;

void FaceTowards(Ent& caveman, sim::FxVec2 target_pos, const Stage& stage) {
    const sim::FxVec2 delta = GetNearestWorldDelta(stage, caveman.GetSimCenter(), target_pos);
    if (delta.x < sim::Scalar::zero()) {
        caveman.facing = Side::Left;
    } else if (delta.x > sim::Scalar::zero()) {
        caveman.facing = Side::Right;
    }
}

void StartIdle(Ent& caveman, State& state) {
    caveman.ai_state = EntAiState::Idle;
    caveman.counter_a = sim::Scalar::from_int(
        state.drng.RandomIntInclusive(kCavemanIdleMinFrames, kCavemanIdleMaxFrames));
    common::DecelerateHorizontallyToStop(caveman, kCavemanWalkAcceleration);
    TrySetAnim(caveman, EntDisplayState::Neutral);
}

void StartWalking(Ent& caveman, const State& state) {
    caveman.ai_state = EntAiState::Patrolling;
    common::AccelerateHorizontallyTowardSpeed(
        caveman,
        state,
        caveman.facing == Side::Left ? -kCavemanWalkSpeed : kCavemanWalkSpeed,
        kCavemanWalkAcceleration
    );
    TrySetAnim(caveman, EntDisplayState::Walk);
}

void StartAttacking(Ent& caveman, const State& state) {
    caveman.ai_state = EntAiState::Pursuing;
    common::AccelerateHorizontallyTowardSpeed(
        caveman,
        state,
        caveman.facing == Side::Left ? -kCavemanAttackSpeed : kCavemanAttackSpeed,
        kCavemanAttackAcceleration
    );
    TrySetAnim(caveman, EntDisplayState::Walk);
}

bool ShouldRunSightScan(const Ent& caveman, std::uint64_t stage_frame) {
    return ((stage_frame + static_cast<std::uint64_t>(caveman.vid.id)) %
            kCavemanSightScanIntervalFrames) == 0;
}

bool CanSeePlayerAhead(
    const Ent& caveman,
    const State& state,
    const Graphics& graphics
) {
    const sim::FxVec2 caveman_center = caveman.GetSimCenter();
    const int direction = caveman.facing == Side::Left ? -1 : 1;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition != EntCondition::Normal) {
            continue;
        }
        const sim::FxVec2 player_center =
            GetNearestWorldPoint(state.stage, caveman_center, player->GetSimCenter());
        const sim::FxVec2 player_delta = player_center - caveman_center;
        if (player_delta.y.abs() > sim::Scalar::from_int(kCavemanSightVerticalTolerance) ||
            player_delta.x.abs() > sim::Scalar::from_int(kCavemanSightDistance)) {
            continue;
        }
        if ((direction < 0 && player_delta.x >= sim::Scalar::zero()) ||
            (direction > 0 && player_delta.x <= sim::Scalar::zero())) {
            continue;
        }
        const WorldRayHit hit = RaycastHorizontal(
            caveman,
            caveman_center,
            direction,
            player_delta.x.abs().trunc_int(),
            state,
            graphics,
            caveman.vid
        );
        if (hit.type == WorldRayHitType::Ent && hit.ent_vid.has_value() &&
            *hit.ent_vid == player->vid) {
            return true;
        }
    }
    return false;
}

void MaybeWallHopWhileIdle(Ent& caveman, const State& state, const Graphics& graphics) {
    if (!caveman.grounded) {
        return;
    }

    const bool touching_left_wall = common::HasWallAheadForGroundWalker(caveman, state, graphics, -1);
    const bool touching_right_wall = common::HasWallAheadForGroundWalker(caveman, state, graphics, 1);
    if (!touching_left_wall && !touching_right_wall) {
        return;
    }

    caveman.vel.y = ToFxScalar(kCavemanWallHopSpeedY);
    caveman.vel.x = caveman.facing == Side::Left ? -ToFxScalar(kCavemanWallHopSpeedX)
                                                 : ToFxScalar(kCavemanWallHopSpeedX);
    caveman.counter_a =
        gfxp::max(sim::Scalar::zero(), caveman.counter_a - sim::Scalar::from_int(10));
}

} // namespace

void StepEntLogicAsCaveman(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Ent& caveman = state.ents.ents[ent_idx];
    if (caveman.last_condition == EntCondition::Stunned &&
        caveman.condition == EntCondition::Normal) {
        StartIdle(caveman, state);
    }
    if (caveman.condition != EntCondition::Normal) {
        return;
    }
    if (caveman.ai_state != EntAiState::Pursuing &&
        ShouldRunSightScan(caveman, state.stage_frame) &&
        CanSeePlayerAhead(caveman, state, graphics)) {
        if (const Ent* const player = FindNearestPlayer(state, caveman.GetSimCenter())) {
            FaceTowards(caveman, player->GetSimCenter(), state.stage);
        }
        if (caveman.grounded) {
            caveman.vel.y = ToFxScalar(kCavemanAlertHopSpeedY);
            caveman.grounded = false;
        }
        (void)PlayEntSoundEmitter(state, caveman, audio_asset_ids::CavemanNotice);
        StartAttacking(caveman, state);
        return;
    }

    if (caveman.ai_state == EntAiState::Pursuing) {
        int direction = caveman.facing == Side::Left ? -1 : 1;
        if (common::HasWallAheadForGroundWalker(caveman, state, graphics, direction)) {
            caveman.facing = caveman.facing == Side::Left ? Side::Right : Side::Left;
            direction = -direction;
        }
        common::AccelerateHorizontallyTowardSpeed(
            caveman,
            state,
            static_cast<float>(direction) * kCavemanAttackSpeed,
            kCavemanAttackAcceleration
        );
        SetMovementFlag(caveman, EntMovementFlag::Running, true);
        SetMovementFlag(caveman, EntMovementFlag::Walking, true);
        TrySetAnim(caveman, EntDisplayState::Walk);
        return;
    }

    if (caveman.ai_state == EntAiState::Idle) {
        common::DecelerateHorizontallyToStop(caveman, kCavemanWalkAcceleration);
        TrySetAnim(caveman, EntDisplayState::Neutral);
        MaybeWallHopWhileIdle(caveman, state, graphics);
        if (caveman.counter_a > sim::Scalar::zero()) {
            caveman.counter_a -= sim::Scalar::from_int(1);
            return;
        }

        caveman.facing =
            state.drng.RandomIntInclusive(0, 1) == 0 ? Side::Left : Side::Right;
        StartWalking(caveman, state);
        return;
    }

    int direction = caveman.facing == Side::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(caveman, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(caveman, state, graphics, direction)) {
        caveman.facing = caveman.facing == Side::Left ? Side::Right : Side::Left;
        direction = -direction;
    }

    if (state.drng.RandomIntInclusive(1, kCavemanIdleChance) == 1) {
        StartIdle(caveman, state);
        return;
    }

    common::AccelerateHorizontallyTowardSpeed(
        caveman,
        state,
        static_cast<float>(direction) * kCavemanWalkSpeed,
        kCavemanWalkAcceleration
    );
    SetMovementFlag(caveman, EntMovementFlag::Walking, true);
    TrySetAnim(caveman, EntDisplayState::Walk);
}

extern const EntSpec kCavemanSpec{
    .type_ = EntType::Caveman,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 3,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .counter_a = EntSpecCounter(static_cast<float>(kCavemanIdleMinFrames)),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::CavemanHurt,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntLogicAsCaveman,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Caveman),
};

} // namespace splonks::ents::caveman

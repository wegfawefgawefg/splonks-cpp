#include "ents/skeleton.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/common/ground_walker.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "on_damage_effects.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <cmath>
#include <memory>

namespace splonks::ents::skeleton {

namespace {

const FVec2 kSkullSize = FVec2::New(7.0F, 4.0F);
const FVec2 kSkeletonSize = FVec2::New(16.0F, 16.0F);
constexpr float kWakeHorizontalDistance = 48.0F;
constexpr float kWakeVerticalAbove = 8.0F;
constexpr float kWakeVerticalBelow = 32.0F;
constexpr float kReturnHorizontalDistance = 96.0F;
constexpr float kReturnVerticalDistance = 64.0F;
constexpr float kSkeletonWalkSpeed = 1.0F;
constexpr float kSkeletonWalkAcceleration = 0.2F;
constexpr float kSkullBreakImpactSpeed = 2.25F;

std::optional<sim::FxVec2> GetNearestPlayerDelta(const Ent& ent, const State& state) {
    const Ent* const player = FindNearestPlayer(state, ent.GetSimCenter(), false);
    if (player == nullptr || player->condition == EntCondition::Dead) {
        return std::nullopt;
    }

    const sim::FxVec2 ent_center = ent.GetSimCenter();
    const sim::FxVec2 player_center = GetNearestWorldPoint(state.stage, ent_center, player->GetSimCenter());
    return player_center - ent_center;
}

void ResizeEntPreservingBottomCenter(Ent& ent, const FVec2& new_size) {
    const sim::FxVec2 bottom_center = ent.pos + sim::FxVec2{
        ent.size.x / sim::Scalar::from_int(2),
        ent.size.y,
    };
    ent.size = sim::ToSimVec2(new_size);
    ent.pos = bottom_center - sim::FxVec2{
        ent.size.x / sim::Scalar::from_int(2),
        ent.size.y,
    };
}

void EnterDormantState(Ent& ent) {
    ResizeEntPreservingBottomCenter(ent, kSkullSize);
    ent.ai_state = EntAiState::Idle;
    ent.hurt_on_contact = false;
    ent.can_be_stomped = false;
    ent.vel = sim::FxVec2::zero();
    ent.acc = sim::FxVec2::zero();
    ent.aframe_animator.loop = true;
    TrySetAnim(ent, EntDisplayState::Neutral);
}

void EnterGettingUpState(Ent& ent) {
    ResizeEntPreservingBottomCenter(ent, kSkeletonSize);
    ent.ai_state = EntAiState::Disturbed;
    ent.hurt_on_contact = false;
    ent.can_be_stomped = false;
    common::DecelerateHorizontallyToStop(ent, kSkeletonWalkAcceleration);
    ent.aframe_animator.loop = false;
    SetAnim(ent, aframe_ids::SkeletonGettingUp);
}

void EnterWalkingState(Ent& ent, const State& state) {
    ResizeEntPreservingBottomCenter(ent, kSkeletonSize);
    ent.ai_state = EntAiState::Patrolling;
    ent.hurt_on_contact = true;
    ent.can_be_stomped = true;
    ent.aframe_animator.loop = true;
    TrySetAnim(ent, EntDisplayState::Walk);
    common::AccelerateHorizontallyTowardSpeed(
        ent,
        state,
        ent.facing == Side::Left ? -kSkeletonWalkSpeed : kSkeletonWalkSpeed,
        kSkeletonWalkAcceleration
    );
}

bool IsPlayerInWakeRange(const Ent& ent, const State& state) {
    const std::optional<sim::FxVec2> player_delta = GetNearestPlayerDelta(ent, state);
    if (!player_delta.has_value()) {
        return false;
    }

    return player_delta->x.abs() <= sim::ToSimScalar(kWakeHorizontalDistance) &&
           player_delta->y >= -sim::ToSimScalar(kWakeVerticalAbove) &&
           player_delta->y <= sim::ToSimScalar(kWakeVerticalBelow);
}

bool IsPlayerOutsideReturnRange(const Ent& ent, const State& state) {
    const std::optional<sim::FxVec2> player_delta = GetNearestPlayerDelta(ent, state);
    if (!player_delta.has_value()) {
        return true;
    }

    return player_delta->x.abs() > sim::ToSimScalar(kReturnHorizontalDistance) ||
           player_delta->y.abs() > sim::ToSimScalar(kReturnVerticalDistance);
}

void FaceNearestPlayerIfAny(Ent& ent, const State& state) {
    const std::optional<sim::FxVec2> player_delta = GetNearestPlayerDelta(ent, state);
    if (!player_delta.has_value()) {
        return;
    }

    if (player_delta->x < sim::Scalar::zero()) {
        ent.facing = Side::Left;
    } else if (player_delta->x > sim::Scalar::zero()) {
        ent.facing = Side::Right;
    }
}

bool IsGroundedInNarrowPit(const Ent& ent, const State& state, const Graphics& graphics) {
    if (!ent.grounded) {
        return false;
    }

    return common::HasWallAheadForGroundWalker(ent, state, graphics, -1) &&
           common::HasWallAheadForGroundWalker(ent, state, graphics, 1);
}

void SpawnSkullBreakEffects(const FVec2& center, State& state) {
    SpawnBreakawayContainerShards(center, state);

    for (int i = 0; i < 2; ++i) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = 14;
        smoke.pos = center;
        smoke.size = FVec2::New(6.0F, 6.0F);
        smoke.alpha = 0.75F;
        smoke.vel = FVec2::New(
            rng::RandomFloat(-0.6F, 0.6F),
            rng::RandomFloat(-1.2F, -0.3F)
        );
        smoke.svel = FVec2::New(0.2F, 0.2F);
        smoke.alpha_vel = -0.05F;
        state.particles.Add(std::move(smoke));
    }
}

void SpawnSkeletonDeathEffects(const FVec2& center, State& state) {
    SpawnBreakawayContainerShards(center, state);

    for (int i = 0; i < 3; ++i) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = 12;
        smoke.pos = center;
        smoke.size = FVec2::New(5.0F, 5.0F);
        smoke.alpha = 0.7F;
        smoke.vel = FVec2::New(
            rng::RandomFloat(-1.0F, 1.0F),
            rng::RandomFloat(-1.5F, -0.5F)
        );
        smoke.svel = FVec2::New(0.15F, 0.15F);
        smoke.alpha_vel = -0.06F;
        state.particles.Add(std::move(smoke));
    }
}

void DropLooseSkull(sim::FxVec2 center, State& state) {
    (void)world_ops::SpawnEnt(state, EntType::Skull, [&](Ent& skull) {
        skull.SetSimCenter(center);
        skull.vel = sim::FxVec2{
            RandomSimScalar(state.drng, sim::Scalar::from_int(-1), sim::Scalar::from_int(1)),
            RandomSimScalar(state.drng, sim::ToSimScalar(-1.8F), sim::ToSimScalar(-0.8F)),
        };
        skull.acc = sim::FxVec2::zero();
    });
}

bool BreakSkull(std::size_t ent_idx, State& state) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& skull = state.ents.ents[ent_idx];
    if (!skull.active || skull.type_ != EntType::Skull || skull.condition == EntCondition::Dead) {
        return false;
    }

    skull.health = 0;
    return true;
}

} // namespace

void StepEntLogicAsSkeleton(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Ent& skeleton = state.ents.ents[ent_idx];
    if (skeleton.condition != EntCondition::Normal) {
        skeleton.hurt_on_contact = false;
        skeleton.can_be_stomped = false;
        return;
    }

    switch (skeleton.ai_state) {
    case EntAiState::Idle:
        skeleton.hurt_on_contact = false;
        common::DecelerateHorizontallyToStop(skeleton, kSkeletonWalkAcceleration);
        if (IsPlayerInWakeRange(skeleton, state)) {
            FaceNearestPlayerIfAny(skeleton, state);
            EnterGettingUpState(skeleton);
        }
        return;
    case EntAiState::Disturbed:
        skeleton.hurt_on_contact = false;
        common::DecelerateHorizontallyToStop(skeleton, kSkeletonWalkAcceleration);
        if (skeleton.aframe_animator.anim_id != aframe_ids::SkeletonGettingUp) {
            EnterGettingUpState(skeleton);
            return;
        }
        if (skeleton.aframe_animator.IsFinished()) {
            EnterWalkingState(skeleton, state);
        }
        return;
    case EntAiState::Patrolling:
    case EntAiState::Returning:
    case EntAiState::Pursuing:
        break;
    }

    if ((skeleton.grounded && IsPlayerOutsideReturnRange(skeleton, state)) ||
        IsGroundedInNarrowPit(skeleton, state, graphics)) {
        EnterDormantState(skeleton);
        return;
    }

    int direction = skeleton.facing == Side::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(skeleton, state, graphics, direction)) {
        skeleton.facing = skeleton.facing == Side::Left ? Side::Right : Side::Left;
        direction = -direction;
    }

    skeleton.hurt_on_contact = true;
    common::AccelerateHorizontallyTowardSpeed(
        skeleton,
        state,
        static_cast<float>(direction) * kSkeletonWalkSpeed,
        kSkeletonWalkAcceleration
    );
    SetMovementFlag(skeleton, EntMovementFlag::Walking, true);
    TrySetAnim(skeleton, EntDisplayState::Walk);
}

void OnDeathAsSkull(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& skull = state.ents.ents[ent_idx];
    SpawnSkullBreakEffects(sim::ToRenderVec2(skull.GetSimCenter()), state);
}

void OnDeathAsSkeleton(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& skeleton = state.ents.ents[ent_idx];
    const sim::FxVec2 center = skeleton.GetSimCenter();
    SpawnSkeletonDeathEffects(sim::ToRenderVec2(center), state);
    DropLooseSkull(center, state);
    (void)world_ops::DeactivateEnt(state, skeleton.vid);
}

bool TryApplySkullTileImpact(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& skull = state.ents.ents[ent_idx];
    if (skull.type_ != EntType::Skull || skull.condition == EntCondition::Dead) {
        return false;
    }
    if (context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return false;
    }
    if (std::abs(context.impact_velocity) < kSkullBreakImpactSpeed) {
        return false;
    }

    return BreakSkull(ent_idx, state);
}

bool TryApplySkullEntImpact(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state
) {
    if (ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& skull = state.ents.ents[ent_idx];
    const Ent& other = state.ents.ents[other_ent_idx];
    if (skull.type_ != EntType::Skull || skull.condition == EntCondition::Dead || !other.active) {
        return false;
    }
    if (other.type_ == EntType::Skull) {
        return false;
    }
    if (skull.thrown_by.has_value() && other.vid == *skull.thrown_by) {
        return false;
    }

    const bool thrown_impact =
        context.phase == common::ContactPhase::SweptEntered && skull.proj_contact_timer > 0;
    const bool blocked_impact =
        context.phase == common::ContactPhase::AttemptedBlocked && context.has_impact &&
        std::abs(context.impact_velocity) >= kSkullBreakImpactSpeed;
    if (!thrown_impact && !blocked_impact) {
        return false;
    }

    return BreakSkull(ent_idx, state);
}

common::ContactResult OnEntContactAsSkull(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    if (!context.mover_vid.has_value() || *context.mover_vid != state.ents.ents[ent_idx].vid) {
        return common::ContactResult{};
    }
    return common::ContactResult{
        .blocks_movement = false,
        .stop_sweep = TryApplySkullEntImpact(ent_idx, other_ent_idx, context, state),
    };
}

common::ContactResult OnTileContactAsSkull(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    return common::ContactResult{
        .blocks_movement = false,
        .stop_sweep = TryApplySkullTileImpact(ent_idx, context, state),
    };
}

extern const EntSpec kSkullSpec{
    .type_ = EntType::Skull,
    .size = EntSpecSize(kSkullSize),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .collide_sound = audio_asset_ids::Thud,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsSkull,
    .on_ent_contact = OnEntContactAsSkull,
    .on_tile_contact = OnTileContactAsSkull,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Skull),
};

extern const EntSpec kSkeletonSpec{
    .type_ = EntType::Skeleton,
    .size = EntSpecSize(kSkullSize),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .collide_sound = audio_asset_ids::Thud,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsSkeleton,
    .step_logic = StepEntLogicAsSkeleton,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Skull),
};

} // namespace splonks::ents::skeleton

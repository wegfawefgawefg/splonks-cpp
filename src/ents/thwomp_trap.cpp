#include "ents/thwomp_trap.hpp"

#include "audio.hpp"
#include "ents/block.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "world_query.hpp"

namespace splonks::ents::thwomp_trap {

namespace {

constexpr float kTriggerDistance = 96.0F;
constexpr float kTriggerHalfWidth = 8.0F;
constexpr float kDropGravity = 0.45F;
constexpr float kDropMaxVelocity = 6.0F;
constexpr float kReturnVelocity = -1.0F;
constexpr int kWaitFrames = 100;
constexpr float kImpactShake = 0.45F;
constexpr float kImpactTileShake = 0.36F;
constexpr float kImpactShakeRadiusTiles = 1.2F;
const sim::Scalar kSimTriggerDistance = ToFxScalar(kTriggerDistance);
const sim::Scalar kSimTriggerHalfWidth = ToFxScalar(kTriggerHalfWidth);

bool HasHomePosition(const Ent& thwomp) {
    return thwomp.point_label_a == PointLabel::Target;
}

void StoreHomePosition(Ent& thwomp) {
    if (HasHomePosition(thwomp)) {
        return;
    }
    thwomp.point_label_a = PointLabel::Target;
    thwomp.point_a = sim::ToPixelIVec2Round(thwomp.pos);
}

sim::Scalar GetHomeY(const Ent& thwomp) {
    return sim::Scalar::from_pixels(thwomp.point_a.y);
}

bool IsDropping(const Ent& thwomp) {
    return thwomp.ai_state == EntAiState::Disturbed;
}

bool IsWaiting(const Ent& thwomp) {
    return thwomp.ai_state == EntAiState::Pursuing;
}

bool IsReturning(const Ent& thwomp) {
    return thwomp.ai_state == EntAiState::Returning;
}

bool ShouldDrop(const Ent& thwomp, const State& state) {
    const sim::FxVec2 thwomp_center = thwomp.GetCenter();
    for (const Ent& ent : state.ents.ents) {
        if (!ent.active || !IsPlayerLikeEntType(ent.type_) ||
            ent.condition == EntCondition::Dead) {
            continue;
        }

        const sim::FxVec2 delta =
            GetNearestWorldDelta(state.stage, thwomp_center, ent.GetCenter());
        if (delta.y <= sim::Scalar::zero() || delta.y > kSimTriggerDistance ||
            delta.x.abs() > kSimTriggerHalfWidth) {
            continue;
        }
        return true;
    }
    return false;
}

void StartDrop(Ent& thwomp) {
    thwomp.ai_state = EntAiState::Disturbed;
    thwomp.vel = sim::FxVec2::zero();
    thwomp.acc = sim::FxVec2::zero();
}

void StartWait(Ent& thwomp, State& state) {
    thwomp.ai_state = EntAiState::Pursuing;
    thwomp.vel = sim::FxVec2::zero();
    thwomp.acc = sim::FxVec2::zero();
    thwomp.counter_a = sim::Scalar::from_int(kWaitFrames);
    AddEntShake(thwomp, kImpactShake);
    AddShake(
        state,
        ToFVec2(thwomp.GetCenter()),
        kImpactTileShake,
        kImpactTileShake * 0.65F,
        0.0F,
        kImpactShakeRadiusTiles,
        thwomp.vid
    );
    (void)PlayEntCenterSoundEmitter(state, thwomp, audio_asset_ids::BoulderHitGround);
}

void StartReturn(Ent& thwomp) {
    thwomp.ai_state = EntAiState::Returning;
    thwomp.vel = sim::FxVec2{sim::Scalar::zero(), ToFxScalar(kReturnVelocity)};
    thwomp.acc = sim::FxVec2::zero();
}

void FinishReturn(Ent& thwomp) {
    thwomp.ai_state = EntAiState::Idle;
    thwomp.pos.y = GetHomeY(thwomp);
    thwomp.vel = sim::FxVec2::zero();
    thwomp.acc = sim::FxVec2::zero();
    thwomp.grounded = false;
    thwomp.collided = false;
}

} // namespace

void StepEntLogicAsThwompTrap(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& thwomp = state.ents.ents[ent_idx];
    SetAnim(thwomp, aframe_ids::ThwompTrap);
    StoreHomePosition(thwomp);

    if (thwomp.condition == EntCondition::Dead) {
        return;
    }

    if (thwomp.ai_state == EntAiState::Idle) {
        thwomp.vel = sim::FxVec2::zero();
        thwomp.acc = sim::FxVec2::zero();
        if (ShouldDrop(thwomp, state)) {
            StartDrop(thwomp);
        }
        return;
    }

    if (IsWaiting(thwomp)) {
        thwomp.vel = sim::FxVec2::zero();
        thwomp.acc = sim::FxVec2::zero();
        thwomp.counter_a -= sim::Scalar::from_int(1);
        if (thwomp.counter_a <= sim::Scalar::zero()) {
            StartReturn(thwomp);
        }
    }
}

void StepEntPhysicsAsThwompTrap(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& thwomp = state.ents.ents[ent_idx];
    if (!IsDropping(thwomp) && !IsReturning(thwomp)) {
        thwomp.collided_last_frame = thwomp.collided;
        thwomp.collided = false;
        thwomp.grounded = false;
        return;
    }

    const bool was_grounded = thwomp.grounded;
    const sim::Scalar pre_vel_y = thwomp.vel.y;

    if (IsDropping(thwomp)) {
        thwomp.acc.y += ToFxScalar(kDropGravity);
    } else {
        thwomp.vel = sim::FxVec2{sim::Scalar::zero(), ToFxScalar(kReturnVelocity)};
        thwomp.acc = sim::FxVec2::zero();
    }

    common::PrePartialEulerStep(ent_idx, state, dt);
    if (IsDropping(thwomp)) {
        thwomp.vel.y = gfxp::clamp(thwomp.vel.y, sim::Scalar::zero(),
                                   ToFxScalar(kDropMaxVelocity));
    }
    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    common::PostPartialEulerStep(ent_idx, state, dt);

    if (IsDropping(thwomp)) {
        const bool hit_bottom = (!was_grounded && thwomp.grounded) ||
                                (pre_vel_y > sim::Scalar::zero() && thwomp.collided &&
                                 thwomp.vel.y == sim::Scalar::zero());
        if (hit_bottom) {
            StartWait(thwomp, state);
        }
        return;
    }

    if (IsReturning(thwomp)) {
        if (thwomp.pos.y <= GetHomeY(thwomp) ||
            (pre_vel_y < sim::Scalar::zero() && thwomp.collided &&
             thwomp.vel.y == sim::Scalar::zero())) {
            FinishReturn(thwomp);
        }
    }
}

extern const EntSpec kThwompTrapSpec{
    .type_ = EntType::ThwompTrap,
    .size = EntSpecSize(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_proj_contact = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Middle,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .damage_vuln = DamageVuln::ExplosionOnly,
    .proj_contact_damage_amount = 0,
    .on_death = block::OnDeathAsBlock,
    .step_logic = StepEntLogicAsThwompTrap,
    .step_physics = StepEntPhysicsAsThwompTrap,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::ThwompTrap),
};

} // namespace splonks::ents::thwomp_trap

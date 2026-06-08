#include "ents/damsel.hpp"

#include "audio.hpp"
#include "ents/basic_exit.hpp"
#include "buying.hpp"
#include "ents/common/common.hpp"
#include "ents/common/ground_walker.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "particles/sprite_particle.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <cmath>
#include <memory>

namespace splonks::ents::damsel {

namespace {

constexpr std::uint64_t kDamselIdleCryIntervalFrames = 1200;
constexpr std::uint64_t kDamselPanicCryIntervalFrames = 360;
constexpr float kRescueKissYOffsetFactor = 0.25F;
constexpr float kRescueKissFloatSpeed = -0.18F;
constexpr std::uint32_t kRescueKissLifetimeFrames = 48;
constexpr float kDamselWalkMinSpeed = 0.1F;
constexpr float kDamselPanicRunSpeed = 1.5F;
constexpr float kDamselRunAcceleration = 0.2F;
constexpr float kDamselHeldReleaseLatch = 1.0F;
constexpr int kDamselRescueHealthGain = 1;

void RefreshCarryStunWhileHeld(Ent& damsel) {
    if (damsel.condition == EntCondition::Dead || !damsel.held_by_vid.has_value()) {
        return;
    }

    damsel.condition = EntCondition::Stunned;
    damsel.stun_timer = common::kDefaultStunTimer;
    TrySetAnim(damsel, EntDisplayState::Stunned);
}

void SpawnRescueKissParticle(const Vec2& pos, State& state) {
    SpriteParticle kiss{};
    kiss.aframe_animator = AFrameAnimator::New(aframe_ids::Kiss);
    kiss.finish_on_anim_end = true;
    kiss.draw_layer = DrawLayer::Foreground;
    kiss.counter = kRescueKissLifetimeFrames;
    kiss.pos = pos;
    kiss.size = Vec2::New(12.0F, 10.0F);
    kiss.rot = 0.0F;
    kiss.alpha = 1.0F;
    kiss.vel = Vec2::New(0.0F, kRescueKissFloatSpeed);
    kiss.svel = Vec2::New(0.0F, 0.0F);
    kiss.rotvel = 0.0F;
    kiss.alpha_vel = -0.01F;
    kiss.acc = Vec2::New(0.0F, 0.0F);
    kiss.sacc = Vec2::New(0.0F, 0.0F);
    kiss.rotacc = 0.0F;
    kiss.alpha_acc = 0.0F;
    state.particles.Add(std::move(kiss));
}

Vec2 GetRescueKissPosForEnt(std::optional<VID> target_vid, const State& state, const Ent& damsel) {
    if (!target_vid.has_value()) {
        return sim::ToRenderVec2(damsel.GetSimCenter());
    }

    const Ent* const target = state.ents.GetEnt(*target_vid);
    if (target == nullptr || !target->active) {
        return sim::ToRenderVec2(damsel.GetSimCenter());
    }

    const sim::AABB target_aabb = target->GetSimAABB();
    return sim::ToRenderVec2(sim::Vec2{
        target_aabb.center().x,
        target_aabb.tl.y + target->size.y * sim::ToSimScalar(kRescueKissYOffsetFactor),
    });
}

Vec2 GetRescueKissPos(const State& state, const Ent& damsel) {
    return GetRescueKissPosForEnt(
        FindNearestPlayerVid(state, damsel.GetSimCenter(), false),
        state,
        damsel
    );
}

void AwardDamselRescueHealthToEnt(std::optional<VID> target_vid, State& state) {
    if (!target_vid.has_value()) {
        return;
    }

    Ent* const target = state.ents.GetEntMut(*target_vid);
    if (target == nullptr || !target->active || target->condition == EntCondition::Dead) {
        return;
    }

    target->health += kDamselRescueHealthGain;
}

void AwardDamselRescueHealth(State& state) {
    AwardDamselRescueHealthToEnt(FindFirstConnectedPlayerVid(state), state);
}

void DetachDamselFromHolder(Ent& damsel, State& state) {
    if (!damsel.held_by_vid.has_value()) {
        return;
    }

    Ent* const holder = state.ents.GetEntMut(*damsel.held_by_vid);
    if (holder != nullptr) {
        if (holder->holding_vid.has_value() && *holder->holding_vid == damsel.vid) {
            holder->holding_vid.reset();
            holder->holding = false;
            holder->holding_timer = kDefaultHoldingTimer;
        }
        if (holder->back_vid.has_value() && *holder->back_vid == damsel.vid) {
            holder->back_vid.reset();
        }
    }

    damsel.held_by_vid.reset();
    damsel.attach_mode = AttachMode::None;
    StopUsingEnt(damsel);
}

void StartIdle(Ent& damsel) {
    damsel.ai_state = EntAiState::Idle;
    common::DecelerateHorizontallyToStop(damsel, kDamselRunAcceleration);
    TrySetAnim(damsel, EntDisplayState::Neutral);
}

void StartPanicRun(Ent& damsel, const State& state) {
    damsel.ai_state = EntAiState::Patrolling;
    const int direction = damsel.facing == Side::Left ? -1 : 1;
    common::AccelerateHorizontallyTowardSpeed(
        damsel,
        state,
        static_cast<float>(direction) * kDamselPanicRunSpeed,
        kDamselRunAcceleration
    );
    TrySetAnim(damsel, EntDisplayState::Walk);
}

void MaybeStartPanicRunFromCarryRelease(Ent& damsel, const State& state) {
    if (damsel.counter_a <= sim::Scalar::zero() || damsel.condition != EntCondition::Normal) {
        return;
    }

    damsel.counter_a = sim::Scalar::zero();
    StartPanicRun(damsel, state);
}

void RescueDamsel(
    std::size_t ent_idx,
    std::optional<VID> rescued_by_vid,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    Ent& damsel = state.ents.ents[ent_idx];
    const Vec2 kiss_pos = GetRescueKissPosForEnt(rescued_by_vid, state, damsel);
    SpawnRescueKissParticle(kiss_pos, state);
    (void)PlayWorldSoundEmitter(state, kiss_pos, audio_asset_ids::Smooch);
    AwardDamselRescueHealthToEnt(rescued_by_vid, state);

    DetachDamselFromHolder(damsel, state);
    damsel.damage_vuln = DamageVuln::Immune;
    damsel.can_collide = false;
    damsel.has_physics = false;
    (void)world_ops::DeactivateEnt(state, damsel.vid);
    state.UpdateSidForEnt(ent_idx, graphics);
}

void KissEnt(std::optional<VID> kissed_by_vid, State& state, const Ent& damsel, Audio& audio) {
    (void)audio;
    const Vec2 kiss_pos = GetRescueKissPosForEnt(kissed_by_vid, state, damsel);
    SpawnRescueKissParticle(kiss_pos, state);
    (void)PlayWorldSoundEmitter(state, kiss_pos, audio_asset_ids::Smooch);
    AwardDamselRescueHealthToEnt(kissed_by_vid, state);
}

bool TryRescueDamsel(std::size_t ent_idx, State& state, const Graphics& graphics, Audio& audio) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& damsel = state.ents.ents[ent_idx];
    if (!damsel.active || damsel.condition == EntCondition::Dead) {
        return false;
    }
    if (!ents::basic_exit::IsEntTouchingBasicExit(damsel, state, graphics)) {
        return false;
    }

    RescueDamsel(
        ent_idx,
        FindNearestPlayerVid(state, damsel.GetSimCenter(), false),
        state,
        graphics,
        audio
    );
    return true;
}


void StepPanicRun(Ent& damsel, const State& state, const Graphics& graphics) {
    if (!damsel.grounded) {
        return;
    }

    int direction = damsel.facing == Side::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(damsel, state, graphics, direction)) {
        damsel.facing = damsel.facing == Side::Left ? Side::Right : Side::Left;
        direction = -direction;
    }

    common::AccelerateHorizontallyTowardSpeed(
        damsel,
        state,
        static_cast<float>(direction) * kDamselPanicRunSpeed,
        kDamselRunAcceleration
    );
    SetMovementFlag(damsel, EntMovementFlag::Running, true);
    SetMovementFlag(damsel, EntMovementFlag::Walking, true);
}

void UpdateDamselAnim(Ent& damsel) {
    if (damsel.condition == EntCondition::Dead || damsel.condition == EntCondition::Stunned) {
        return;
    }

    if (damsel.aframe_animator.anim_id == aframe_ids::DamselCry) {
        if (!damsel.aframe_animator.IsFinished()) {
            return;
        }
        damsel.aframe_animator.loop = true;
    }

    const bool walking = damsel.grounded &&
                         damsel.vel.x.abs() >= sim::ToSimScalar(kDamselWalkMinSpeed);
    TrySetAnim(damsel, walking ? EntDisplayState::Walk : EntDisplayState::Neutral);
}

bool ShouldPlayAmbientCry(const Ent& damsel, std::uint64_t stage_frame) {
    const std::uint64_t interval = damsel.ai_state == EntAiState::Patrolling
                                       ? kDamselPanicCryIntervalFrames
                                       : kDamselIdleCryIntervalFrames;
    return ((stage_frame + static_cast<std::uint64_t>(damsel.vid.id)) % interval) == 0;
}

} // namespace

bool BuyDamsel(
    std::size_t ent_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    (void)graphics;

    if (ent_idx >= state.ents.ents.size() ||
        buyer_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& damsel = state.ents.ents[ent_idx];
    if (!damsel.active || damsel.condition == EntCondition::Dead) {
        return false;
    }
    const std::uint32_t price = damsel.buyable.display_quantity;
    if (!TrySpendMoney(buyer_idx, price, state, audio)) {
        return false;
    }

    KissEnt(state.ents.ents[buyer_idx].vid, state, damsel, audio);
    return true;
}

void StepEntLogicAsDamsel(
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

    Ent& damsel = state.ents.ents[ent_idx];
    if (!damsel.active || damsel.condition == EntCondition::Dead) {
        return;
    }

    if (TryRescueDamsel(ent_idx, state, graphics, audio)) {
        return;
    }

    RefreshCarryStunWhileHeld(damsel);
    if (damsel.held_by_vid.has_value()) {
        damsel.counter_a = sim::ToSimScalar(kDamselHeldReleaseLatch);
        damsel.ai_state = EntAiState::Idle;
        return;
    }

    if (damsel.last_condition == EntCondition::Stunned &&
        damsel.condition == EntCondition::Normal) {
        StartPanicRun(damsel, state);
    }

    MaybeStartPanicRunFromCarryRelease(damsel, state);

    if (damsel.condition == EntCondition::Normal &&
        damsel.ai_state == EntAiState::Patrolling && !damsel.held_by_vid.has_value()) {
        StepPanicRun(damsel, state, graphics);
    }

    if (damsel.condition == EntCondition::Normal &&
        ShouldPlayAmbientCry(damsel, state.stage_frame)) {
        (void)PlayEntSoundEmitter(state, damsel, audio_asset_ids::DamselAmbientCry);
        SetAnim(damsel, aframe_ids::DamselCry);
        damsel.aframe_animator.loop = false;
    }

    UpdateDamselAnim(damsel);
}

extern const EntSpec kDamselSpec{
    .type_ = EntType::Damsel,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 3,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = true,
    .stun_recovers_on_ground = true,
    .stun_recovers_while_held = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .buyable = Buyable{.on_try_buy = BuyDamsel},
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::DamselHurt,
    .step_logic = StepEntLogicAsDamsel,
    .alignment = Alignment::Ally,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Damsel),
};

} // namespace splonks::ents::damsel

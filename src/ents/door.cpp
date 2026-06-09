#include "ents/door.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "ents/common/common.hpp"
#include "ent/spec.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace splonks::ents::door {

namespace {

constexpr float kDropStartVelocity = 0.0F;
constexpr float kDropGravity = 0.006F;
constexpr float kDropMaxVelocity = 0.20F;
constexpr float kRightSensorMinX = 8.0F;
constexpr float kRightSensorMaxX = 40.0F;
constexpr float kRightSensorMinY = 28.0F;
constexpr float kRightSensorMaxY = 96.0F;
constexpr float kRumbleFrames = 60.0F;
constexpr float kRumbleDoorShake = 0.05F;
constexpr float kMovingDoorShake = 0.10F;
constexpr float kDropStartDoorShake = 0.12F;
constexpr float kSealDoorShake = 0.22F;
constexpr float kSealForegroundShakeAmount = 0.18F;
constexpr float kSealBackgroundShakeAmount = 0.12F;
constexpr float kSealShakeRadiusTiles = 1.1F;
constexpr float kDoorRumbleVolumeScale = 1.0F;
constexpr int kTopSmokeIntervalFrames = 8;
constexpr int kSealSmokeCount = 8;
constexpr int kSealShardCount = 5;
constexpr float kDoorSealWaitFrames = 100.0F;
const sim::Scalar kSimRightSensorMinX = sim::ToSimScalar(kRightSensorMinX);
const sim::Scalar kSimRightSensorMaxX = sim::ToSimScalar(kRightSensorMaxX);
const sim::Scalar kSimRightSensorMinY = sim::ToSimScalar(kRightSensorMinY);
const sim::Scalar kSimRightSensorMaxY = sim::ToSimScalar(kRightSensorMaxY);

bool IsRumbling(const Ent& door) {
    return door.ai_state == EntAiState::Pursuing;
}

bool IsDropping(const Ent& door) {
    return door.ai_state == EntAiState::Disturbed;
}

bool IsSealed(const Ent& door) {
    return door.ai_state == EntAiState::Returning;
}

bool HasTargetTopY(const Ent& door) {
    return door.point_label_a == PointLabel::Target;
}

sim::Scalar GetTargetTopY(const Ent& door) {
    return sim::Scalar::from_pixels(door.point_a.y);
}

float GetMoveDirection(const Ent& door) {
    return door.threshold_a < sim::Scalar::zero() ? -1.0F : 1.0F;
}

FVec2 GetTopEmitPos(const Ent& door) {
    const sim::AABB aabb = door.GetSimAABB();
    return sim::ToRenderVec2(sim::FxVec2{aabb.center().x, aabb.tl.y});
}

FVec2 GetBottomEmitPos(const Ent& door) {
    const sim::AABB aabb = door.GetSimAABB();
    return sim::ToRenderVec2(sim::FxVec2{aabb.center().x, aabb.br.y});
}

FVec2 GetTrailingEmitPos(const Ent& door) {
    return GetMoveDirection(door) > 0.0F ? GetTopEmitPos(door) : GetBottomEmitPos(door);
}

FVec2 GetLeadingEmitPos(const Ent& door) {
    return GetMoveDirection(door) > 0.0F ? GetBottomEmitPos(door) : GetTopEmitPos(door);
}

void SetDoorShake(Ent& door, float amount) {
    door.shake = std::max(door.shake, sim::ToSimScalar(amount));
}

void SpawnSmokeParticle(
    State& state,
    const FVec2& pos,
    const FVec2& vel,
    float min_size,
    float max_size,
    std::uint32_t min_lifetime,
    std::uint32_t max_lifetime
) {
    SpriteParticle smoke{};
    smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
    smoke.draw_layer = DrawLayer::Foreground;
    smoke.counter = static_cast<std::uint32_t>(
        rng::RandomIntExclusive(static_cast<int>(min_lifetime), static_cast<int>(max_lifetime)));
    smoke.pos = pos + FVec2::New(rng::RandomFloat(-1.5F, 1.5F), rng::RandomFloat(-1.0F, 1.0F));
    const float size = rng::RandomFloat(min_size, max_size);
    smoke.size = FVec2::New(size, size);
    smoke.rot = rng::RandomFloat(0.0F, 360.0F);
    smoke.alpha = rng::RandomFloat(0.60F, 0.90F);
    smoke.vel = vel + FVec2::New(rng::RandomFloat(-0.18F, 0.18F), rng::RandomFloat(-0.12F, 0.12F));
    smoke.svel = FVec2::New(rng::RandomFloat(0.01F, 0.025F), rng::RandomFloat(0.01F, 0.025F));
    smoke.rotvel = rng::RandomFloat(-0.25F, 0.25F);
    smoke.alpha_vel = -0.02F;
    smoke.acc = FVec2::New(0.0F, -0.005F);
    smoke.sacc = FVec2::New(0.0F, 0.0F);
    smoke.rotacc = 0.0F;
    smoke.alpha_acc = -0.002F;
    state.particles.Add(std::move(smoke));
}

void SpawnSpinnySealShard(State& state, const FVec2& pos) {
    SpriteParticle shard{};
    shard.aframe_animator = AFrameAnimator::New(aframe_ids::LittleBrownShard);
    shard.draw_layer = DrawLayer::Foreground;
    shard.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 34));
    shard.pos = pos + FVec2::New(rng::RandomFloat(-5.0F, 5.0F), rng::RandomFloat(-1.5F, 1.5F));
    const float size = rng::RandomFloat(2.0F, 4.0F);
    shard.size = FVec2::New(size, size);
    shard.rot = rng::RandomFloat(0.0F, 360.0F);
    shard.alpha = 1.0F;
    shard.vel = FVec2::New(rng::RandomFloat(-1.4F, 1.4F), rng::RandomFloat(-2.2F, -0.7F));
    shard.svel = FVec2::New(0.0F, 0.0F);
    shard.rotvel = rng::RandomFloat(-0.85F, 0.85F);
    shard.alpha_vel = -0.025F;
    shard.acc = FVec2::New(0.0F, 0.14F);
    shard.sacc = FVec2::New(0.0F, 0.0F);
    shard.rotacc = 0.0F;
    shard.alpha_acc = -0.003F;
    state.particles.Add(std::move(shard));
}

void SpawnTopSmoke(State& state, const Ent& door) {
    SpawnSmokeParticle(
        state,
        GetTrailingEmitPos(door),
        FVec2::New(0.0F, GetMoveDirection(door) > 0.0F
                            ? rng::RandomFloat(-0.55F, -0.18F)
                            : rng::RandomFloat(0.18F, 0.55F)),
        3.0F,
        5.5F,
        16,
        30
    );
}

void SpawnSealParticles(State& state, const Ent& door) {
    const FVec2 bottom = GetLeadingEmitPos(door);
    for (int i = 0; i < kSealSmokeCount; ++i) {
        const float direction = i % 2 == 0 ? -1.0F : 1.0F;
        SpawnSmokeParticle(
            state,
            bottom + FVec2::New(direction * 5.5F, 0.0F),
            FVec2::New(
                direction * rng::RandomFloat(0.45F, 1.25F),
                GetMoveDirection(door) > 0.0F ? rng::RandomFloat(-0.65F, -0.25F)
                                              : rng::RandomFloat(0.25F, 0.65F)
            ),
            4.0F,
            7.0F,
            18,
            38
        );
    }
    for (int i = 0; i < kSealShardCount; ++i) {
        SpawnSpinnySealShard(state, bottom);
    }
}

bool ShouldStartDrop(const Ent& door, const State& state) {
    const sim::FxVec2 door_center = door.GetSimCenter();
    for (const Ent& ent : state.ents.ents) {
        if (!ent.active || !IsPlayerLikeEntType(ent.type_) ||
            ent.condition == EntCondition::Dead) {
            continue;
        }
        const sim::FxVec2 delta =
            GetNearestWorldDelta(state.stage, door_center, ent.GetSimCenter());
        if (delta.x >= kSimRightSensorMinX && delta.x <= kSimRightSensorMaxX &&
            delta.y >= kSimRightSensorMinY &&
            delta.y <= kSimRightSensorMaxY) {
            return true;
        }
    }
    return false;
}

void MaintainDoorRumbleSound(Ent& door, State& state);

void StartRumble(Ent& door, State& state) {
    door.ai_state = EntAiState::Pursuing;
    door.vel = sim::FxVec2::zero();
    door.acc = sim::FxVec2::zero();
    door.counter_b = sim::ToSimScalar(kRumbleFrames);
    SetDoorShake(door, kRumbleDoorShake);
    MaintainDoorRumbleSound(door, state);
}

void MaintainDoorRumbleSound(Ent& door, State& state) {
    (void)EnsureAttachedLoopingSoundEmitter(
        state,
        door.vid,
        door.vid,
        FVec2::New(0.0F, door.GetSize().y * 0.5F),
        audio_asset_ids::BoulderRoll,
        kDoorRumbleVolumeScale
    );
}

void StartDrop(Ent& door, State& state) {
    door.ai_state = EntAiState::Disturbed;
    door.render_enabled = true;
    door.vel = sim::FxVec2{
        sim::Scalar::zero(),
        sim::ToSimScalar(GetMoveDirection(door) * kDropStartVelocity),
    };
    door.acc = sim::FxVec2::zero();
    SetDoorShake(door, kDropStartDoorShake);
    AudioEmitterPlayParams params;
    params.volume_scale = 0.85F;
    params.owner_ent_vid = door.vid;
    (void)PlayAttachedSoundEmitter(
        state,
        door.vid,
        FVec2::New(0.0F, door.GetSize().y * 0.5F),
        audio_asset_ids::BoulderLatch,
        params
    );
}

void SealDoor(Ent& door, State& state, Audio& audio) {
    door.ai_state = EntAiState::Returning;
    door.vel = sim::FxVec2::zero();
    door.acc = sim::FxVec2::zero();
    door.counter_a = sim::ToSimScalar(kDoorSealWaitFrames);
    (void)StopOwnedSoundEmitter(
        state,
        audio,
        door.vid,
        audio_asset_ids::BoulderRoll,
        AudioEmitterPlaybackMode::Looping
    );
    SpawnSealParticles(state, door);
    SetDoorShake(door, kSealDoorShake);
    AddShake(
        state,
        GetLeadingEmitPos(door),
        kSealForegroundShakeAmount,
        kSealBackgroundShakeAmount,
        0.0F,
        kSealShakeRadiusTiles,
        door.vid
    );
    (void)PlayEntCenterSoundEmitter(state, door, audio_asset_ids::BoulderHitGround);
}

common::ContactResult CrushEntOnDoorContact(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    if (audio == nullptr || ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return {};
    }

    const Ent& door = state.ents.ents[ent_idx];
    if (!IsDropping(door) || !context.mover_vid.has_value() || *context.mover_vid != door.vid ||
        context.phase != common::ContactPhase::SweptEntered) {
        return {};
    }

    Ent& other_ent = state.ents.ents[other_ent_idx];
    if (!other_ent.active || !other_ent.can_collide || other_ent.impassable) {
        return {};
    }

    (void)common::TryDamageEnt(other_ent_idx, state, *audio, DamageType::Crush, 1);
    return common::ContactResult{.stop_sweep = true};
}

} // namespace

void StepEntLogicAsDoor(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& door = state.ents.ents[ent_idx];
    SetAnim(door, aframe_ids::IdleTrapDoor);

    if (door.condition == EntCondition::Dead) {
        return;
    }

    if (door.ai_state == EntAiState::Idle) {
        door.vel = sim::FxVec2::zero();
        if (ShouldStartDrop(door, state)) {
            StartRumble(door, state);
        }
        return;
    }

    if (IsRumbling(door)) {
        door.vel = sim::FxVec2::zero();
        SetDoorShake(door, kRumbleDoorShake);
        MaintainDoorRumbleSound(door, state);
        if (((state.stage_frame + door.vid.id) % (kTopSmokeIntervalFrames * 2)) == 0U) {
            SpawnTopSmoke(state, door);
        }
        door.counter_b -= sim::Scalar::from_int(1);
        if (door.counter_b <= sim::Scalar::zero()) {
            StartDrop(door, state);
        }
        return;
    }

    if (IsDropping(door)) {
        SetDoorShake(door, kMovingDoorShake);
        MaintainDoorRumbleSound(door, state);
        if (((state.stage_frame + door.vid.id) % kTopSmokeIntervalFrames) == 0U) {
            SpawnTopSmoke(state, door);
        }
        return;
    }

    if (IsSealed(door) && door.counter_a > sim::Scalar::zero()) {
        door.counter_a -= sim::Scalar::from_int(1);
    }

    (void)audio;
}

void StepEntPhysicsAsDoor(
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

    Ent& door = state.ents.ents[ent_idx];
    if (!IsDropping(door)) {
        door.collided_last_frame = door.collided;
        door.collided = false;
        door.grounded = false;
        return;
    }

    const bool was_grounded = door.grounded;
    const sim::Scalar pre_vel_y = door.vel.y;
    const float move_direction = GetMoveDirection(door);
    door.acc.y += sim::ToSimScalar(move_direction * kDropGravity);
    common::PrePartialEulerStep(ent_idx, state, dt);
    if (move_direction > 0.0F) {
        door.vel.y = gfxp::clamp(door.vel.y, sim::Scalar::zero(),
                                 sim::ToSimScalar(kDropMaxVelocity));
    } else {
        door.vel.y = gfxp::clamp(door.vel.y, -sim::ToSimScalar(kDropMaxVelocity),
                                 sim::Scalar::zero());
    }
    if (HasTargetTopY(door)) {
        common::DoEntCollisions(ent_idx, state, graphics, audio);
    } else {
        common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    }
    common::PostPartialEulerStep(ent_idx, state, dt);

    if (HasTargetTopY(door)) {
        const sim::Scalar target_top_y = GetTargetTopY(door);
        if ((move_direction > 0.0F && door.pos.y >= target_top_y) ||
            (move_direction < 0.0F && door.pos.y <= target_top_y)) {
            door.pos.y = target_top_y;
            SealDoor(door, state, audio);
        }
        return;
    }

    const bool hit_bottom =
        (move_direction > 0.0F && !was_grounded && door.grounded) ||
        (pre_vel_y != sim::Scalar::zero() && door.collided &&
         door.vel.y == sim::Scalar::zero());
    if (hit_bottom) {
        SealDoor(door, state, audio);
    }
}

extern const EntSpec kDoorSpec{
    .type_ = EntType::Door,
    .size = EntSpecSize(16.0F, 32.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_proj_contact = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Middle,
    .render_enabled = false,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::ExplosionOnly,
    .proj_contact_damage_amount = 0,
    .step_logic = StepEntLogicAsDoor,
    .step_physics = StepEntPhysicsAsDoor,
    .on_ent_contact = CrushEntOnDoorContact,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::IdleTrapDoor),
};

} // namespace splonks::ents::door

#include "entities/door.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "entities/common/common.hpp"
#include "entity/archetype.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace splonks::entities::door {

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

bool IsRumbling(const Entity& door) {
    return door.ai_state == EntityAiState::Pursuing;
}

bool IsDropping(const Entity& door) {
    return door.ai_state == EntityAiState::Disturbed;
}

bool IsSealed(const Entity& door) {
    return door.ai_state == EntityAiState::Returning;
}

bool HasTargetTopY(const Entity& door) {
    return door.point_label_a == PointLabel::Target;
}

float GetTargetTopY(const Entity& door) {
    return static_cast<float>(door.point_a.y);
}

float GetMoveDirection(const Entity& door) {
    return door.threshold_a < 0.0F ? -1.0F : 1.0F;
}

Vec2 GetTopEmitPos(const Entity& door) {
    const AABB aabb = door.GetAABB();
    return Vec2::New((aabb.tl.x + aabb.br.x) * 0.5F, aabb.tl.y);
}

Vec2 GetBottomEmitPos(const Entity& door) {
    const AABB aabb = door.GetAABB();
    return Vec2::New((aabb.tl.x + aabb.br.x) * 0.5F, aabb.br.y);
}

Vec2 GetTrailingEmitPos(const Entity& door) {
    return GetMoveDirection(door) > 0.0F ? GetTopEmitPos(door) : GetBottomEmitPos(door);
}

Vec2 GetLeadingEmitPos(const Entity& door) {
    return GetMoveDirection(door) > 0.0F ? GetBottomEmitPos(door) : GetTopEmitPos(door);
}

void SetDoorShake(Entity& door, float amount) {
    door.shake = std::max(door.shake, amount);
}

void SpawnSmokeParticle(
    State& state,
    const Vec2& pos,
    const Vec2& vel,
    float min_size,
    float max_size,
    std::uint32_t min_lifetime,
    std::uint32_t max_lifetime
) {
    SpriteParticle smoke{};
    smoke.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
    smoke.draw_layer = DrawLayer::Foreground;
    smoke.counter = static_cast<std::uint32_t>(
        rng::RandomIntExclusive(static_cast<int>(min_lifetime), static_cast<int>(max_lifetime)));
    smoke.pos = pos + Vec2::New(rng::RandomFloat(-1.5F, 1.5F), rng::RandomFloat(-1.0F, 1.0F));
    const float size = rng::RandomFloat(min_size, max_size);
    smoke.size = Vec2::New(size, size);
    smoke.rot = rng::RandomFloat(0.0F, 360.0F);
    smoke.alpha = rng::RandomFloat(0.60F, 0.90F);
    smoke.vel = vel + Vec2::New(rng::RandomFloat(-0.18F, 0.18F), rng::RandomFloat(-0.12F, 0.12F));
    smoke.svel = Vec2::New(rng::RandomFloat(0.01F, 0.025F), rng::RandomFloat(0.01F, 0.025F));
    smoke.rotvel = rng::RandomFloat(-0.25F, 0.25F);
    smoke.alpha_vel = -0.02F;
    smoke.acc = Vec2::New(0.0F, -0.005F);
    smoke.sacc = Vec2::New(0.0F, 0.0F);
    smoke.rotacc = 0.0F;
    smoke.alpha_acc = -0.002F;
    state.particles.Add(std::move(smoke));
}

void SpawnSpinnySealShard(State& state, const Vec2& pos) {
    SpriteParticle shard{};
    shard.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleBrownShard);
    shard.draw_layer = DrawLayer::Foreground;
    shard.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 34));
    shard.pos = pos + Vec2::New(rng::RandomFloat(-5.0F, 5.0F), rng::RandomFloat(-1.5F, 1.5F));
    const float size = rng::RandomFloat(2.0F, 4.0F);
    shard.size = Vec2::New(size, size);
    shard.rot = rng::RandomFloat(0.0F, 360.0F);
    shard.alpha = 1.0F;
    shard.vel = Vec2::New(rng::RandomFloat(-1.4F, 1.4F), rng::RandomFloat(-2.2F, -0.7F));
    shard.svel = Vec2::New(0.0F, 0.0F);
    shard.rotvel = rng::RandomFloat(-0.85F, 0.85F);
    shard.alpha_vel = -0.025F;
    shard.acc = Vec2::New(0.0F, 0.14F);
    shard.sacc = Vec2::New(0.0F, 0.0F);
    shard.rotacc = 0.0F;
    shard.alpha_acc = -0.003F;
    state.particles.Add(std::move(shard));
}

void SpawnTopSmoke(State& state, const Entity& door) {
    SpawnSmokeParticle(
        state,
        GetTrailingEmitPos(door),
        Vec2::New(0.0F, GetMoveDirection(door) > 0.0F
                            ? rng::RandomFloat(-0.55F, -0.18F)
                            : rng::RandomFloat(0.18F, 0.55F)),
        3.0F,
        5.5F,
        16,
        30
    );
}

void SpawnSealParticles(State& state, const Entity& door) {
    const Vec2 bottom = GetLeadingEmitPos(door);
    for (int i = 0; i < kSealSmokeCount; ++i) {
        const float direction = i % 2 == 0 ? -1.0F : 1.0F;
        SpawnSmokeParticle(
            state,
            bottom + Vec2::New(direction * 5.5F, 0.0F),
            Vec2::New(
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

bool ShouldStartDrop(const Entity& door, const State& state) {
    const Vec2 door_center = door.GetCenter();
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active || !IsPlayerLikeEntityType(entity.type_) ||
            entity.condition == EntityCondition::Dead) {
            continue;
        }
        const Vec2 delta = GetNearestWorldDelta(state.stage, door_center, entity.GetCenter());
        if (delta.x >= kRightSensorMinX && delta.x <= kRightSensorMaxX &&
            delta.y >= kRightSensorMinY &&
            delta.y <= kRightSensorMaxY) {
            return true;
        }
    }
    return false;
}

void MaintainDoorRumbleSound(Entity& door, State& state);

void StartRumble(Entity& door, State& state) {
    door.ai_state = EntityAiState::Pursuing;
    door.vel = Vec2::New(0.0F, 0.0F);
    door.acc = Vec2::New(0.0F, 0.0F);
    door.counter_b = kRumbleFrames;
    SetDoorShake(door, kRumbleDoorShake);
    MaintainDoorRumbleSound(door, state);
}

void MaintainDoorRumbleSound(Entity& door, State& state) {
    (void)EnsureAttachedLoopingSoundEmitter(
        state,
        door.vid,
        door.vid,
        Vec2::New(0.0F, door.size.y * 0.5F),
        audio_asset_ids::BoulderRoll,
        kDoorRumbleVolumeScale
    );
}

void StartDrop(Entity& door, State& state) {
    door.ai_state = EntityAiState::Disturbed;
    door.render_enabled = true;
    door.vel = Vec2::New(0.0F, GetMoveDirection(door) * kDropStartVelocity);
    door.acc = Vec2::New(0.0F, 0.0F);
    SetDoorShake(door, kDropStartDoorShake);
    AudioEmitterPlayParams params;
    params.volume_scale = 0.85F;
    params.owner_entity_vid = door.vid;
    (void)PlayAttachedSoundEmitter(
        state,
        door.vid,
        Vec2::New(0.0F, door.size.y * 0.5F),
        audio_asset_ids::BoulderLatch,
        params
    );
}

void SealDoor(Entity& door, State& state, Audio& audio) {
    door.ai_state = EntityAiState::Returning;
    door.vel = Vec2::New(0.0F, 0.0F);
    door.acc = Vec2::New(0.0F, 0.0F);
    door.counter_a = kDoorSealWaitFrames;
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
    (void)PlayEntityCenterSoundEmitter(state, door, audio_asset_ids::BoulderHitGround);
}

common::ContactResolution CrushEntityOnDoorContact(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    if (audio == nullptr || entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return {};
    }

    const Entity& door = state.entity_manager.entities[entity_idx];
    if (!IsDropping(door) || !context.mover_vid.has_value() || *context.mover_vid != door.vid ||
        context.phase != common::ContactPhase::SweptEntered) {
        return {};
    }

    Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    if (!other_entity.active || !other_entity.can_collide || other_entity.impassable) {
        return {};
    }

    (void)common::TryDamageEntity(other_entity_idx, state, *audio, DamageType::Crush, 1);
    return common::ContactResolution{.stop_sweep = true};
}

} // namespace

void StepEntityLogicAsDoor(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& door = state.entity_manager.entities[entity_idx];
    SetAnimation(door, frame_data_ids::IdleTrapDoor);

    if (door.condition == EntityCondition::Dead) {
        return;
    }

    if (door.ai_state == EntityAiState::Idle) {
        door.vel = Vec2::New(0.0F, 0.0F);
        if (ShouldStartDrop(door, state)) {
            StartRumble(door, state);
        }
        return;
    }

    if (IsRumbling(door)) {
        door.vel = Vec2::New(0.0F, 0.0F);
        SetDoorShake(door, kRumbleDoorShake);
        MaintainDoorRumbleSound(door, state);
        if (((state.stage_frame + door.vid.id) % (kTopSmokeIntervalFrames * 2)) == 0U) {
            SpawnTopSmoke(state, door);
        }
        door.counter_b -= 1.0F;
        if (door.counter_b <= 0.0F) {
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

    if (IsSealed(door) && door.counter_a > 0.0F) {
        door.counter_a -= 1.0F;
    }

    (void)audio;
}

void StepEntityPhysicsAsDoor(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& door = state.entity_manager.entities[entity_idx];
    if (!IsDropping(door)) {
        door.collided_last_frame = door.collided;
        door.collided = false;
        door.grounded = false;
        return;
    }

    const bool was_grounded = door.grounded;
    const float pre_vel_y = door.vel.y;
    const float move_direction = GetMoveDirection(door);
    door.acc.y += move_direction * kDropGravity;
    common::PrePartialEulerStep(entity_idx, state, dt);
    if (move_direction > 0.0F) {
        door.vel.y = std::clamp(door.vel.y, 0.0F, kDropMaxVelocity);
    } else {
        door.vel.y = std::clamp(door.vel.y, -kDropMaxVelocity, 0.0F);
    }
    if (HasTargetTopY(door)) {
        common::DoEntityCollisions(entity_idx, state, graphics, audio);
    } else {
        common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    }
    common::PostPartialEulerStep(entity_idx, state, dt);

    if (HasTargetTopY(door)) {
        const float target_top_y = GetTargetTopY(door);
        if ((move_direction > 0.0F && door.pos.y >= target_top_y) ||
            (move_direction < 0.0F && door.pos.y <= target_top_y)) {
            door.pos.y = target_top_y;
            SealDoor(door, state, audio);
        }
        return;
    }

    const bool hit_bottom =
        (move_direction > 0.0F && !was_grounded && door.grounded) ||
        (pre_vel_y != 0.0F && door.collided && door.vel.y == 0.0F);
    if (hit_bottom) {
        SealDoor(door, state, audio);
    }
}

extern const EntityArchetype kDoorArchetype{
    .type_ = EntityType::Door,
    .size = Vec2::New(16.0F, 32.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_projectile_contact = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Middle,
    .render_enabled = false,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::ExplosionOnly,
    .projectile_contact_damage_amount = 0,
    .step_logic = StepEntityLogicAsDoor,
    .step_physics = StepEntityPhysicsAsDoor,
    .on_entity_contact = CrushEntityOnDoorContact,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::IdleTrapDoor),
};

} // namespace splonks::entities::door

#include "ents/boulder.hpp"

#include "audio_emitters.hpp"
#include "audio.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "sim/fxp.hpp"
#include "stage_break.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <memory>

namespace splonks::ents::boulder {

namespace {

constexpr float kBoulderRollVelocity = 9.0F;
constexpr float kBoulderRestFrames = 5.0F;
constexpr float kBoulderTrailSmokeDistInterval = 12.0F;
constexpr float kBoulderTrailPebbleDistInterval = 12.0F;
constexpr float kBoulderImpactSoundCooldownFrames = 8.0F;
constexpr float kBoulderRollingSoundVolumeScale = 0.9F;
constexpr float kBoulderRollSoundDistInterval = 96.0F;
constexpr float kBoulderRollingSpeedThreshold = 0.01F;
constexpr float kBoulderRollingShakeForegroundAmount = 0.76F;
constexpr float kBoulderRollingShakeBackgroundAmount = 0.48F;
constexpr float kBoulderRollingShakeRadiusTiles = 1.6F;
constexpr float kBoulderBreakShakeForegroundAmount = 1.15F;
constexpr float kBoulderBreakShakeBackgroundAmount = 0.90F;
constexpr float kBoulderBreakShakeEntAmount = 0.95F;
constexpr float kBoulderBreakShakeRadiusTiles = 2.6F;
constexpr float kBoulderWallHitShakeForegroundAmount = 1.25F;
constexpr float kBoulderWallHitShakeBackgroundAmount = 1.00F;
constexpr float kBoulderWallHitShakeEntAmount = 1.05F;
constexpr float kBoulderWallHitShakeRadiusTiles = 2.8F;
constexpr float kBoulderGroundSlamShakeForegroundAmount = 1.10F;
constexpr float kBoulderGroundSlamShakeBackgroundAmount = 0.85F;
constexpr float kBoulderGroundSlamShakeEntAmount = 1.85F;
constexpr float kBoulderGroundSlamShakeRadiusTiles = 3.4F;
constexpr AFrameId kBoulderAnimId = HashAFrameIdConstexpr("boulder");
constexpr AFrameId kBoulderRollAnimId = HashAFrameIdConstexpr("boulder_roll");
constexpr AFrameId kBoulderParticleAnimId = kBoulderAnimId;

FVec2 GetBoulderBottomCenter(const Ent& boulder);
FVec2 GetBoulderFrontFaceCenter(const Ent& boulder);

sim::FxAABB GetLeadingBreakStrip(const Ent& boulder) {
    const sim::FxAABB aabb = boulder.GetAABB();
    if (boulder.facing == Side::Right) {
        return sim::FxAABB::from_corners(
            sim::FxVec2{aabb.br.x + sim::Scalar::from_pixels(1), aabb.tl.y},
            sim::FxVec2{aabb.br.x + sim::Scalar::from_pixels(1), aabb.br.y}
        );
    }
    return sim::FxAABB::from_corners(
        sim::FxVec2{aabb.tl.x - sim::Scalar::from_pixels(1), aabb.tl.y},
        sim::FxVec2{aabb.tl.x - sim::Scalar::from_pixels(1), aabb.br.y}
    );
}

bool WouldBreakAnyTiles(sim::FxAABB area, const State& state) {
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, area)) {
        if (tile_query.tile == nullptr) {
            continue;
        }

        const Tile tile = *tile_query.tile;
        if (tile != Tile::Air) {
            return true;
        }
    }
    return false;
}

void StepRollingSound(State& state, Ent& boulder) {
    boulder.travel_sound_countdown -= boulder.dist_traveled_this_frame;
    if (boulder.travel_sound_countdown >= sim::Scalar::zero()) {
        return;
    }

    boulder.travel_sound_countdown =
        sim::Scalar::from_int(static_cast<std::int32_t>(kBoulderRollSoundDistInterval));
    AudioEmitterPlayParams params;
    params.volume_scale = kBoulderRollingSoundVolumeScale;
    params.owner_ent_vid = boulder.vid;
    (void)PlayAttachedSoundEmitter(
        state,
        boulder.vid,
        FVec2::New(0.0F, ToFVec2(boulder.size).y * 0.5F),
        audio_asset_ids::BoulderRoll,
        params
    );
}

void SpawnBoulderTrailSmoke(State& state, const FVec2& pos, Side facing) {
    for (int i = 0; i < 2; ++i) {
        SpriteParticle effect{};
        effect.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 32));
        effect.pos = pos + FVec2::New(
                  rng::RandomFloat(-2.0F, 2.0F),
                  rng::RandomFloat(-2.0F, 2.0F)
              );
        effect.size = FVec2::New(rng::RandomFloat(3.0F, 6.0F), rng::RandomFloat(3.0F, 6.0F));
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = rng::RandomFloat(0.6F, 0.9F);
        effect.vel = FVec2::New(
            facing == Side::Right ? rng::RandomFloat(-0.8F, -0.2F)
                                         : rng::RandomFloat(0.2F, 0.8F),
            rng::RandomFloat(-1.0F, -0.2F)
        );
        effect.svel = FVec2::New(rng::RandomFloat(0.01F, 0.03F), rng::RandomFloat(0.01F, 0.03F));
        effect.rotvel = rng::RandomFloat(-0.2F, 0.2F);
        effect.alpha_vel = -0.02F;
        effect.acc = FVec2::New(0.0F, 0.01F);
        effect.sacc = FVec2::New(0.0F, 0.0F);
        effect.rotacc = 0.0F;
        effect.alpha_acc = -0.003F;
        state.particles.Add(std::move(effect));
    }
}

void SpawnBoulderTrailPebbles(State& state, const FVec2& pos, Side facing) {
    const int count = rng::RandomIntExclusive(1, 3);
    for (int i = 0; i < count; ++i) {
        SpriteParticle effect{};
        effect.aframe_animator = AFrameAnimator::New(kBoulderParticleAnimId);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 34));
        effect.pos = pos + FVec2::New(
                  rng::RandomFloat(-1.0F, 1.0F),
                  rng::RandomFloat(-1.0F, 1.0F)
              );
        const float size = rng::RandomFloat(2.0F, 5.0F);
        effect.size = FVec2::New(size, size);
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = 1.0F;
        effect.vel = FVec2::New(
            facing == Side::Right ? rng::RandomFloat(0.8F, 1.8F)
                                         : rng::RandomFloat(-1.8F, -0.8F),
            rng::RandomFloat(-1.8F, -0.6F)
        );
        effect.svel = FVec2::New(0.0F, 0.0F);
        effect.rotvel = rng::RandomFloat(-0.5F, 0.5F);
        effect.alpha_vel = -0.03F;
        effect.acc = FVec2::New(0.0F, 0.16F);
        effect.sacc = FVec2::New(0.0F, 0.0F);
        effect.rotacc = 0.0F;
        effect.alpha_acc = -0.003F;
        state.particles.Add(std::move(effect));
    }
}

void UpdateBoulderAnim(Ent& boulder) {
    const bool is_rolling =
        boulder.ai_state == EntAiState::Disturbed &&
        boulder.vel.x.abs() > ToFxScalar(kBoulderRollingSpeedThreshold);
    SetAnim(boulder, is_rolling ? kBoulderRollAnimId : kBoulderAnimId);
}

void PlayBoulderImpactSoundIfReady(Ent& boulder, State& state) {
    if (boulder.counter_a > sim::Scalar::zero()) {
        return;
    }
    boulder.counter_a = ToFxScalar(kBoulderImpactSoundCooldownFrames);
    state.frame_pause += 2;
    (void)PlayWorldSoundEmitter(state, GetBoulderBottomCenter(boulder), audio_asset_ids::BoulderHitGround);
}

FVec2 GetBoulderTrailingBottomCorner(const Ent& boulder) {
    const sim::FxAABB aabb = boulder.GetAABB();
    return ToFVec2(boulder.facing == Side::Right
                                 ? sim::FxVec2{aabb.tl.x, aabb.br.y}
                                 : sim::FxVec2{aabb.br.x, aabb.br.y});
}

FVec2 GetBoulderLeadingBottomCorner(const Ent& boulder) {
    const sim::FxAABB aabb = boulder.GetAABB();
    return ToFVec2(boulder.facing == Side::Right
                                 ? sim::FxVec2{aabb.br.x, aabb.br.y}
                                 : sim::FxVec2{aabb.tl.x, aabb.br.y});
}

FVec2 GetBoulderBottomCenter(const Ent& boulder) {
    const sim::FxAABB aabb = boulder.GetAABB();
    return ToFVec2(sim::FxVec2{aabb.center().x, aabb.br.y});
}

FVec2 GetBoulderFrontFaceCenter(const Ent& boulder) {
    const sim::FxAABB aabb = boulder.GetAABB();
    return ToFVec2(boulder.facing == Side::Right
                                 ? sim::FxVec2{aabb.br.x, aabb.center().y}
                                 : sim::FxVec2{aabb.tl.x, aabb.center().y});
}

void AddBoulderRollingShake(State& state, const Ent& boulder) {
    AddShake(
        state,
        GetBoulderBottomCenter(boulder),
        kBoulderRollingShakeForegroundAmount,
        kBoulderRollingShakeBackgroundAmount,
        0.0F,
        kBoulderRollingShakeRadiusTiles
    );
}

void AddBoulderBreakShake(State& state, const Ent& boulder) {
    const FVec2 center = GetBoulderFrontFaceCenter(boulder);
    AddShake(
        state,
        center,
        kBoulderBreakShakeForegroundAmount,
        kBoulderBreakShakeBackgroundAmount,
        0.0F,
        kBoulderBreakShakeRadiusTiles
    );
    AddShake(
        state,
        center,
        0.0F,
        0.0F,
        kBoulderBreakShakeEntAmount,
        kBoulderBreakShakeRadiusTiles,
        boulder.vid
    );
}

void AddBoulderWallHitShake(State& state, const Ent& boulder) {
    const FVec2 center = GetBoulderFrontFaceCenter(boulder);
    AddShake(
        state,
        center,
        kBoulderWallHitShakeForegroundAmount,
        kBoulderWallHitShakeBackgroundAmount,
        0.0F,
        kBoulderWallHitShakeRadiusTiles
    );
    AddShake(
        state,
        center,
        0.0F,
        0.0F,
        kBoulderWallHitShakeEntAmount,
        kBoulderWallHitShakeRadiusTiles
    );
}

void AddBoulderGroundSlamShake(State& state, const Ent& boulder) {
    const FVec2 center = GetBoulderBottomCenter(boulder);
    AddShake(
        state,
        center,
        kBoulderGroundSlamShakeForegroundAmount,
        kBoulderGroundSlamShakeBackgroundAmount,
        0.0F,
        kBoulderGroundSlamShakeRadiusTiles
    );
    AddShake(
        state,
        center,
        0.0F,
        0.0F,
        kBoulderGroundSlamShakeEntAmount,
        kBoulderGroundSlamShakeRadiusTiles
    );
}

} // namespace

extern const EntSpec kBoulderSpec{
    .type_ = EntType::Boulder,
    .size = EntSpecSize(28.0F, 28.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_stomp = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .affected_by_ground_friction = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Right,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::ExplosionOnly,
    .on_death = OnDeathAsBoulder,
    .step_logic = StepEntLogicAsBoulder,
    .step_physics = StepEntPhysicsAsBoulder,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(kBoulderAnimId),
};

void SpawnBoulderBreakEffects(const FVec2& center, State& state) {
    for (int i = 0; i < 20; ++i) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::BigSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(28, 56));
        smoke.pos = center + FVec2::New(
                 rng::RandomFloat(-4.0F, 4.0F),
                 rng::RandomFloat(-4.0F, 4.0F)
             );
        smoke.size = FVec2::New(rng::RandomFloat(6.0F, 14.0F), rng::RandomFloat(6.0F, 14.0F));
        smoke.rot = rng::RandomFloat(0.0F, 360.0F);
        smoke.alpha = 1.0F;
        smoke.vel = FVec2::New(
            rng::RandomFloat(-1.2F, 1.2F),
            rng::RandomFloat(-2.2F, -0.4F)
        );
        smoke.svel = FVec2::New(rng::RandomFloat(0.05F, 0.12F), rng::RandomFloat(0.05F, 0.12F));
        smoke.rotvel = rng::RandomFloat(-0.3F, 0.3F);
        smoke.alpha_vel = -0.015F;
        smoke.acc = FVec2::New(0.0F, 0.10F);
        smoke.sacc = FVec2::New(0.0F, 0.0F);
        smoke.rotacc = 0.0F;
        smoke.alpha_acc = -0.002F;
        state.particles.Add(std::move(smoke));
    }

    for (int i = 0; i < 16; ++i) {
        SpriteParticle shard{};
        shard.aframe_animator = AFrameAnimator::New(kBoulderParticleAnimId);
        shard.draw_layer = DrawLayer::Foreground;
        shard.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(30, 60));
        shard.pos = center + FVec2::New(
                 rng::RandomFloat(-3.0F, 3.0F),
                 rng::RandomFloat(-3.0F, 3.0F)
             );
        const float size = rng::RandomFloat(5.0F, 16.0F);
        shard.size = FVec2::New(size, size);
        shard.rot = rng::RandomFloat(0.0F, 360.0F);
        shard.alpha = 1.0F;
        shard.vel = FVec2::New(
            rng::RandomFloat(-3.5F, 3.5F),
            rng::RandomFloat(-5.5F, -1.8F)
        );
        shard.svel = FVec2::New(0.0F, 0.0F);
        shard.rotvel = rng::RandomFloat(-0.6F, 0.6F);
        shard.alpha_vel = -0.012F;
        shard.acc = FVec2::New(0.0F, 0.22F);
        shard.sacc = FVec2::New(0.0F, 0.0F);
        shard.rotacc = 0.0F;
        shard.alpha_acc = -0.002F;
        state.particles.Add(std::move(shard));
    }
}

void OnDeathAsBoulder(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }
    Ent& boulder = state.ents.ents[ent_idx];
    SpawnBoulderBreakEffects(ToFVec2(boulder.GetCenter()), state);
    (void)world_ops::DeactivateEnt(state, boulder.vid);
}

void StepEntLogicAsBoulder(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Ent& boulder = state.ents.ents[ent_idx];
    boulder.max_speed = ToFxScalar(kBoulderRollVelocity);

    if (boulder.ai_state == EntAiState::Idle && boulder.grounded) {
        boulder.ai_state = EntAiState::Disturbed;
        boulder.travel_sound_countdown = sim::Scalar::zero();
        boulder.point_a = ToIVec2(ToFVec2(boulder.pos));
        boulder.counter_b = sim::Scalar::zero();
        boulder.counter_c = sim::Scalar::zero();
        boulder.counter_d = sim::Scalar::zero();
    }

    if (boulder.ai_state != EntAiState::Disturbed) {
        UpdateBoulderAnim(boulder);
        return;
    }

    boulder.vel.x = boulder.facing == Side::Right ? ToFxScalar(kBoulderRollVelocity)
                                                  : -ToFxScalar(kBoulderRollVelocity);
    UpdateBoulderAnim(boulder);
}

void StepEntPhysicsAsBoulder(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    Ent& boulder = state.ents.ents[ent_idx];
    const bool was_grounded = boulder.grounded;
    const sim::Scalar pre_physics_vel_x = boulder.vel.x;
    if (boulder.counter_a > sim::Scalar::zero()) {
        boulder.counter_a -= sim::Scalar::from_int(1);
        if (boulder.counter_a < sim::Scalar::zero()) {
            boulder.counter_a = sim::Scalar::zero();
        }
    }
    if (boulder.ai_state == EntAiState::Disturbed) {
        const sim::FxAABB break_strip = GetLeadingBreakStrip(boulder);
        const bool will_break_tiles = WouldBreakAnyTiles(break_strip, state);
        if (will_break_tiles && boulder.counter_a <= sim::Scalar::zero()) {
            (void)PlayWorldSoundEmitter(
                state,
                GetBoulderFrontFaceCenter(boulder),
                audio_asset_ids::BoulderTileCrash
            );
            boulder.counter_a = ToFxScalar(kBoulderImpactSoundCooldownFrames);
        }
        if (will_break_tiles) {
            AddBoulderBreakShake(state, boulder);
            BreakStageTilesInRectWc(break_strip, state, audio);
        }
    }

    common::StepStandardPhysics(ent_idx, state, graphics, audio, dt);

    const bool landed_this_frame = !was_grounded && boulder.grounded;
    if (landed_this_frame) {
        PlayBoulderImpactSoundIfReady(boulder, state);
        AddBoulderGroundSlamShake(state, boulder);
    }

    if (boulder.ai_state == EntAiState::Disturbed) {
        const bool hard_stopped_this_frame =
            pre_physics_vel_x.abs() > ToFxScalar(kBoulderRollingSpeedThreshold) &&
            boulder.vel.x.abs() <= ToFxScalar(kBoulderRollingSpeedThreshold) &&
            boulder.grounded &&
            boulder.dist_traveled_this_frame <= sim::Scalar::zero();
        if (hard_stopped_this_frame) {
                PlayBoulderImpactSoundIfReady(boulder, state);
            AddBoulderWallHitShake(state, boulder);
        }
        if (boulder.grounded && boulder.dist_traveled_this_frame > sim::Scalar::zero()) {
            StepRollingSound(state, boulder);
            AddBoulderRollingShake(state, boulder);
            const sim::Scalar dist_traveled = boulder.dist_traveled_this_frame;
            boulder.counter_c -= dist_traveled;
            while (boulder.counter_c <= sim::Scalar::zero()) {
                boulder.counter_c += ToFxScalar(kBoulderTrailSmokeDistInterval);
                SpawnBoulderTrailSmoke(
                    state,
                    GetBoulderTrailingBottomCorner(boulder),
                    boulder.facing
                );
            }

            boulder.counter_d -= dist_traveled;
            while (boulder.counter_d <= sim::Scalar::zero()) {
                boulder.counter_d += ToFxScalar(kBoulderTrailPebbleDistInterval);
                SpawnBoulderTrailPebbles(
                    state,
                    GetBoulderLeadingBottomCorner(boulder),
                    boulder.facing
                );
            }
        }

        const IVec2 current_pos = sim::ToPixelIVec2Round(boulder.pos);
        if (current_pos == boulder.point_a) {
            boulder.counter_b += sim::Scalar::from_int(1);
        } else {
            boulder.point_a = current_pos;
            boulder.counter_b = sim::Scalar::zero();
        }

        if (boulder.counter_b >= ToFxScalar(kBoulderRestFrames)) {
                boulder.ai_state = EntAiState::Returning;
            boulder.vel.x = sim::Scalar::zero();
        }
    }

    UpdateBoulderAnim(boulder);
}

} // namespace splonks::ents::boulder

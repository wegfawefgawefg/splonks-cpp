#include "entities/boulder.hpp"

#include "audio.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "stage_break.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <memory>

namespace splonks::entities::boulder {

namespace {

constexpr float kBoulderRollVelocity = 9.0F;
constexpr float kBoulderRestFrames = 5.0F;
constexpr float kBoulderRollSoundDistInterval = 96.0F;
constexpr float kBoulderTrailSmokeDistInterval = 12.0F;
constexpr float kBoulderTrailPebbleDistInterval = 12.0F;
constexpr float kBoulderImpactSoundCooldownFrames = 8.0F;
constexpr float kBoulderRollingSpeedThreshold = 0.01F;
constexpr FrameDataId kBoulderAnimationId = HashFrameDataIdConstexpr("boulder");
constexpr FrameDataId kBoulderRollAnimationId = HashFrameDataIdConstexpr("boulder_roll");
constexpr FrameDataId kBoulderParticleAnimationId = kBoulderAnimationId;

AABB GetLeadingBreakStrip(const Entity& boulder) {
    const AABB aabb = boulder.GetAABB();
    if (boulder.facing == LeftOrRight::Right) {
        return AABB::New(
            Vec2::New(aabb.br.x + 1.0F, aabb.tl.y),
            Vec2::New(aabb.br.x + 1.0F, aabb.br.y)
        );
    }
    return AABB::New(
        Vec2::New(aabb.tl.x - 1.0F, aabb.tl.y),
        Vec2::New(aabb.tl.x - 1.0F, aabb.br.y)
    );
}

bool WouldBreakAnyTiles(const AABB& area, const State& state) {
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, area)) {
        if (tile_query.tile == nullptr) {
            continue;
        }

        const Tile tile = *tile_query.tile;
        if (tile != Tile::Air && tile != Tile::Exit) {
            return true;
        }
    }
    return false;
}

void StepRollingSound(Entity& boulder, Audio& audio) {
    boulder.travel_sound_countdown -= boulder.dist_traveled_this_frame;
    if (boulder.travel_sound_countdown >= 0.0F) {
        return;
    }

    boulder.travel_sound_countdown = kBoulderRollSoundDistInterval;
    audio.PlaySoundEffect(SoundEffect::BoulderRoll);
}

void SpawnBoulderTrailSmoke(State& state, const Vec2& pos, LeftOrRight facing) {
    for (int i = 0; i < 2; ++i) {
        auto effect = std::make_unique<UltraDynamicParticle>();
        effect->frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 32));
        effect->pos = pos + Vec2::New(
                  rng::RandomFloat(-2.0F, 2.0F),
                  rng::RandomFloat(-2.0F, 2.0F)
              );
        effect->size = Vec2::New(rng::RandomFloat(3.0F, 6.0F), rng::RandomFloat(3.0F, 6.0F));
        effect->rot = rng::RandomFloat(0.0F, 360.0F);
        effect->alpha = rng::RandomFloat(0.6F, 0.9F);
        effect->vel = Vec2::New(
            facing == LeftOrRight::Right ? rng::RandomFloat(-0.8F, -0.2F)
                                         : rng::RandomFloat(0.2F, 0.8F),
            rng::RandomFloat(-1.0F, -0.2F)
        );
        effect->svel = Vec2::New(rng::RandomFloat(0.01F, 0.03F), rng::RandomFloat(0.01F, 0.03F));
        effect->rotvel = rng::RandomFloat(-0.2F, 0.2F);
        effect->alpha_vel = -0.02F;
        effect->acc = Vec2::New(0.0F, 0.01F);
        effect->sacc = Vec2::New(0.0F, 0.0F);
        effect->rotacc = 0.0F;
        effect->alpha_acc = -0.003F;
        state.particles.Add(std::move(effect));
    }
}

void SpawnBoulderTrailPebbles(State& state, const Vec2& pos, LeftOrRight facing) {
    const int count = rng::RandomIntExclusive(1, 3);
    for (int i = 0; i < count; ++i) {
        auto effect = std::make_unique<UltraDynamicParticle>();
        effect->frame_data_animator = FrameDataAnimator::New(kBoulderParticleAnimationId);
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 34));
        effect->pos = pos + Vec2::New(
                  rng::RandomFloat(-1.0F, 1.0F),
                  rng::RandomFloat(-1.0F, 1.0F)
              );
        const float size = rng::RandomFloat(2.0F, 5.0F);
        effect->size = Vec2::New(size, size);
        effect->rot = rng::RandomFloat(0.0F, 360.0F);
        effect->alpha = 1.0F;
        effect->vel = Vec2::New(
            facing == LeftOrRight::Right ? rng::RandomFloat(0.8F, 1.8F)
                                         : rng::RandomFloat(-1.8F, -0.8F),
            rng::RandomFloat(-1.8F, -0.6F)
        );
        effect->svel = Vec2::New(0.0F, 0.0F);
        effect->rotvel = rng::RandomFloat(-0.5F, 0.5F);
        effect->alpha_vel = -0.03F;
        effect->acc = Vec2::New(0.0F, 0.16F);
        effect->sacc = Vec2::New(0.0F, 0.0F);
        effect->rotacc = 0.0F;
        effect->alpha_acc = -0.003F;
        state.particles.Add(std::move(effect));
    }
}

void UpdateBoulderAnimation(Entity& boulder) {
    const bool is_rolling =
        boulder.ai_state == EntityAiState::Disturbed &&
        std::abs(boulder.vel.x) > kBoulderRollingSpeedThreshold;
    SetAnimation(boulder, is_rolling ? kBoulderRollAnimationId : kBoulderAnimationId);
}

void PlayBoulderImpactSoundIfReady(Entity& boulder, State& state, Audio& audio) {
    if (boulder.counter_a > 0.0F) {
        return;
    }
    boulder.counter_a = kBoulderImpactSoundCooldownFrames;
    state.frame_pause += 2;
    audio.PlaySoundEffect(SoundEffect::BoulderHitGround);
}

Vec2 GetBoulderTrailingBottomCorner(const Entity& boulder) {
    const AABB aabb = boulder.GetAABB();
    return boulder.facing == LeftOrRight::Right
               ? Vec2::New(aabb.tl.x, aabb.br.y)
               : Vec2::New(aabb.br.x, aabb.br.y);
}

Vec2 GetBoulderLeadingBottomCorner(const Entity& boulder) {
    const AABB aabb = boulder.GetAABB();
    return boulder.facing == LeftOrRight::Right
               ? Vec2::New(aabb.br.x, aabb.br.y)
               : Vec2::New(aabb.tl.x, aabb.br.y);
}

} // namespace

extern const EntityArchetype kBoulderArchetype{
    .type_ = EntityType::Boulder,
    .size = Vec2::New(28.0F, 28.0F),
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
    .has_ground_friction = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Right,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::ExplosionOnly,
    .on_death = OnDeathAsBoulder,
    .step_logic = StepEntityLogicAsBoulder,
    .step_physics = StepEntityPhysicsAsBoulder,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(kBoulderAnimationId),
};

void SpawnBoulderBreakEffects(const Vec2& center, State& state) {
    for (int i = 0; i < 20; ++i) {
        auto smoke = std::make_unique<UltraDynamicParticle>();
        smoke->frame_data_animator = FrameDataAnimator::New(frame_data_ids::BigSmoke);
        smoke->draw_layer = DrawLayer::Foreground;
        smoke->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(28, 56));
        smoke->pos = center + Vec2::New(
                 rng::RandomFloat(-4.0F, 4.0F),
                 rng::RandomFloat(-4.0F, 4.0F)
             );
        smoke->size = Vec2::New(rng::RandomFloat(6.0F, 14.0F), rng::RandomFloat(6.0F, 14.0F));
        smoke->rot = rng::RandomFloat(0.0F, 360.0F);
        smoke->alpha = 1.0F;
        smoke->vel = Vec2::New(
            rng::RandomFloat(-1.2F, 1.2F),
            rng::RandomFloat(-2.2F, -0.4F)
        );
        smoke->svel = Vec2::New(rng::RandomFloat(0.05F, 0.12F), rng::RandomFloat(0.05F, 0.12F));
        smoke->rotvel = rng::RandomFloat(-0.3F, 0.3F);
        smoke->alpha_vel = -0.015F;
        smoke->acc = Vec2::New(0.0F, 0.10F);
        smoke->sacc = Vec2::New(0.0F, 0.0F);
        smoke->rotacc = 0.0F;
        smoke->alpha_acc = -0.002F;
        state.particles.Add(std::move(smoke));
    }

    for (int i = 0; i < 16; ++i) {
        auto shard = std::make_unique<UltraDynamicParticle>();
        shard->frame_data_animator = FrameDataAnimator::New(kBoulderParticleAnimationId);
        shard->draw_layer = DrawLayer::Foreground;
        shard->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(30, 60));
        shard->pos = center + Vec2::New(
                 rng::RandomFloat(-3.0F, 3.0F),
                 rng::RandomFloat(-3.0F, 3.0F)
             );
        const float size = rng::RandomFloat(5.0F, 16.0F);
        shard->size = Vec2::New(size, size);
        shard->rot = rng::RandomFloat(0.0F, 360.0F);
        shard->alpha = 1.0F;
        shard->vel = Vec2::New(
            rng::RandomFloat(-3.5F, 3.5F),
            rng::RandomFloat(-5.5F, -1.8F)
        );
        shard->svel = Vec2::New(0.0F, 0.0F);
        shard->rotvel = rng::RandomFloat(-0.6F, 0.6F);
        shard->alpha_vel = -0.012F;
        shard->acc = Vec2::New(0.0F, 0.22F);
        shard->sacc = Vec2::New(0.0F, 0.0F);
        shard->rotacc = 0.0F;
        shard->alpha_acc = -0.002F;
        state.particles.Add(std::move(shard));
    }
}

void OnDeathAsBoulder(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }
    SpawnBoulderBreakEffects(state.entity_manager.entities[entity_idx].GetCenter(), state);
    state.entity_manager.SetInactive(entity_idx);
}

void StepEntityLogicAsBoulder(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& boulder = state.entity_manager.entities[entity_idx];
    boulder.max_speed = kBoulderRollVelocity;

    if (boulder.ai_state == EntityAiState::Idle && boulder.grounded) {
        boulder.ai_state = EntityAiState::Disturbed;
        boulder.travel_sound_countdown = 0.0F;
        boulder.point_a = ToIVec2(boulder.pos);
        boulder.counter_b = 0.0F;
        boulder.counter_c = 0.0F;
        boulder.counter_d = 0.0F;
    }

    if (boulder.ai_state != EntityAiState::Disturbed) {
        UpdateBoulderAnimation(boulder);
        return;
    }

    boulder.vel.x = boulder.facing == LeftOrRight::Right ? kBoulderRollVelocity : -kBoulderRollVelocity;
    StepRollingSound(boulder, audio);
    UpdateBoulderAnimation(boulder);
}

void StepEntityPhysicsAsBoulder(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    Entity& boulder = state.entity_manager.entities[entity_idx];
    const bool was_grounded = boulder.grounded;
    const float pre_physics_vel_x = boulder.vel.x;
    if (boulder.counter_a > 0.0F) {
        boulder.counter_a -= 1.0F;
        if (boulder.counter_a < 0.0F) {
            boulder.counter_a = 0.0F;
        }
    }
    if (boulder.ai_state == EntityAiState::Disturbed) {
        const AABB break_strip = GetLeadingBreakStrip(boulder);
        const bool will_break_tiles = WouldBreakAnyTiles(break_strip, state);
        if (will_break_tiles && boulder.counter_a <= 0.0F) {
            audio.PlaySoundEffect(SoundEffect::BoulderTileCrash);
            boulder.counter_a = kBoulderImpactSoundCooldownFrames;
        }
        if (will_break_tiles) {
            BreakStageTilesInRectWc(break_strip, state, audio);
        }
    }

    common::StepStandardPhysics(entity_idx, state, graphics, audio, dt);

    if (boulder.ai_state == EntityAiState::Disturbed) {
        const bool landed_this_frame = !was_grounded && boulder.grounded;
        const bool hard_stopped_this_frame =
            std::abs(pre_physics_vel_x) > kBoulderRollingSpeedThreshold &&
            std::abs(boulder.vel.x) <= kBoulderRollingSpeedThreshold &&
            boulder.grounded &&
            boulder.dist_traveled_this_frame <= 0.0F;
        if (landed_this_frame || hard_stopped_this_frame) {
            PlayBoulderImpactSoundIfReady(boulder, state, audio);
        }
        if (boulder.grounded && boulder.dist_traveled_this_frame > 0.0F) {
            boulder.counter_c -= boulder.dist_traveled_this_frame;
            while (boulder.counter_c <= 0.0F) {
                boulder.counter_c += kBoulderTrailSmokeDistInterval;
                SpawnBoulderTrailSmoke(
                    state,
                    GetBoulderTrailingBottomCorner(boulder),
                    boulder.facing
                );
            }

            boulder.counter_d -= boulder.dist_traveled_this_frame;
            while (boulder.counter_d <= 0.0F) {
                boulder.counter_d += kBoulderTrailPebbleDistInterval;
                SpawnBoulderTrailPebbles(
                    state,
                    GetBoulderLeadingBottomCorner(boulder),
                    boulder.facing
                );
            }
        }

        const IVec2 current_pos = ToIVec2(boulder.pos);
        if (current_pos == boulder.point_a) {
            boulder.counter_b += 1.0F;
        } else {
            boulder.point_a = current_pos;
            boulder.counter_b = 0.0F;
        }

        if (boulder.counter_b >= kBoulderRestFrames) {
            boulder.ai_state = EntityAiState::Returning;
            boulder.vel.x = 0.0F;
        }
    }

    UpdateBoulderAnimation(boulder);
}

} // namespace splonks::entities::boulder

#include "entities/cobra.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "entities/common/ground_walker.hpp"
#include "frame_data_id.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <cmath>
#include <memory>
#include <optional>

namespace splonks::entities::cobra {

namespace {

constexpr float kCobraWalkSpeed = 0.85F;
constexpr int kCobraIdleMinFrames = 18;
constexpr int kCobraIdleMaxFrames = 42;
constexpr int kCobraIdleChance = 120;
constexpr float kCobraSightVerticalTolerance = 18.0F;
constexpr int kCobraSightDistance = 120;
constexpr std::uint64_t kCobraSightScanIntervalFrames = 12;
constexpr float kCobraSpitVelocityX = 2.2F;
constexpr float kCobraSpitVelocityY = -1.9F;
constexpr int kCobraSpitCooldownMinFrames = 48;
constexpr int kCobraSpitCooldownMaxFrames = 72;
constexpr float kCobraSpitLifetimeFrames = 120.0F;
constexpr float kCobraSpitTrailIntervalFrames = 2.0F;
constexpr unsigned int kCobraVenomDamage = 1;

void StartIdle(Entity& cobra, int min_frames = kCobraIdleMinFrames, int max_frames = kCobraIdleMaxFrames) {
    cobra.ai_state = EntityAiState::Idle;
    cobra.counter_a = static_cast<float>(rng::RandomIntInclusive(min_frames, max_frames));
    cobra.vel.x = 0.0F;
    TrySetAnimation(cobra, EntityDisplayState::Neutral);
}

void StartWalking(Entity& cobra) {
    cobra.ai_state = EntityAiState::Patrolling;
    cobra.vel.x = cobra.facing == LeftOrRight::Left ? -kCobraWalkSpeed : kCobraWalkSpeed;
    TrySetAnimation(cobra, EntityDisplayState::Walk);
}

void FaceTowards(Entity& cobra, const Vec2& target_pos, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, cobra.GetCenter(), target_pos);
    if (delta.x < 0.0F) {
        cobra.facing = LeftOrRight::Left;
    } else if (delta.x > 0.0F) {
        cobra.facing = LeftOrRight::Right;
    }
}

bool ShouldRunSightScan(const Entity& cobra, std::uint64_t stage_frame) {
    return ((stage_frame + static_cast<std::uint64_t>(cobra.vid.id)) %
            kCobraSightScanIntervalFrames) == 0;
}

bool CanSeePlayerAhead(const Entity& cobra, const State& state, const Graphics& graphics) {
    if (!state.player_vid.has_value()) {
        return false;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition != EntityCondition::Normal) {
        return false;
    }

    const Vec2 spit_origin = common::GetEmitPointForEntity(cobra, graphics, cobra.GetCenter());
    const Vec2 player_center = GetNearestWorldPoint(state.stage, spit_origin, player->GetCenter());
    const Vec2 player_delta = player_center - spit_origin;
    if (std::abs(player_delta.y) > kCobraSightVerticalTolerance ||
        std::abs(player_delta.x) > static_cast<float>(kCobraSightDistance)) {
        return false;
    }

    const int direction = cobra.facing == LeftOrRight::Left ? -1 : 1;
    if ((direction < 0 && player_delta.x >= 0.0F) || (direction > 0 && player_delta.x <= 0.0F)) {
        return false;
    }

    const WorldRayHit hit = RaycastHorizontal(
        cobra,
        spit_origin,
        direction,
        static_cast<int>(std::abs(player_delta.x)),
        state,
        graphics,
        cobra.vid
    );
    return hit.type == WorldRayHitType::Entity && hit.entity_vid.has_value() && *hit.entity_vid == player->vid;
}

void SpawnSpitParticle(State& state, const Vec2& pos, const Vec2& vel, float alpha, float size_jitter) {
    SpriteParticle particle{};
    particle.frame_data_animator = FrameDataAnimator::New(frame_data_ids::CobraSpit);
    particle.draw_layer = DrawLayer::Foreground;
    particle.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(8, 16));
    particle.pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
    particle.size = Vec2::New(
        4.0F + rng::RandomFloat(-size_jitter, size_jitter),
        3.0F + rng::RandomFloat(-size_jitter, size_jitter)
    );
    particle.rot = rng::RandomFloat(-20.0F, 20.0F);
    particle.alpha = alpha;
    particle.vel = vel;
    particle.svel = Vec2::New(0.03F, 0.03F);
    particle.rotvel = rng::RandomFloat(-2.0F, 2.0F);
    particle.alpha_vel = -0.05F;
    particle.acc = Vec2::New(0.0F, 0.12F);
    particle.sacc = Vec2::New(0.0F, 0.0F);
    particle.rotacc = 0.0F;
    particle.alpha_acc = -0.003F;
    state.particles.Add(std::move(particle));
}

void SpawnSpitSpray(State& state, const Vec2& origin, int direction) {
    for (int i = 0; i < 7; ++i) {
        SpawnSpitParticle(
            state,
            origin,
            Vec2::New(
                rng::RandomFloat(0.35F, 1.8F) * static_cast<float>(direction),
                rng::RandomFloat(-0.9F, 0.25F)
            ),
            rng::RandomFloat(0.75F, 1.0F),
            1.0F
        );
    }
}

void SpawnSpitTrail(State& state, const Vec2& origin, const Vec2& base_vel) {
    for (int i = 0; i < 2; ++i) {
        SpawnSpitParticle(
            state,
            origin,
            Vec2::New(
                base_vel.x * rng::RandomFloat(-0.15F, 0.05F),
                base_vel.y * rng::RandomFloat(-0.15F, 0.05F)
            ) + Vec2::New(rng::RandomFloat(-0.1F, 0.1F), rng::RandomFloat(-0.1F, 0.1F)),
            rng::RandomFloat(0.45F, 0.75F),
            0.6F
        );
    }
}

void SpawnSpitImpact(State& state, const Vec2& origin) {
    for (int i = 0; i < 4; ++i) {
        SpawnSpitParticle(
            state,
            origin,
            Vec2::New(rng::RandomFloat(-0.8F, 0.8F), rng::RandomFloat(-0.8F, 0.15F)),
            rng::RandomFloat(0.6F, 0.95F),
            0.8F
        );
    }
}

Entity* SpawnCobraSpitEntity(State& state, const Vec2& pos) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const spit = state.entity_manager.GetEntityMut(*vid);
    if (spit == nullptr) {
        return nullptr;
    }

    SetEntityAs(*spit, EntityType::CobraSpit);
    spit->SetCenter(pos);
    return spit;
}

void FireCobraSpit(std::size_t entity_idx, State& state, Graphics& graphics) {
    Entity& cobra = state.entity_manager.entities[entity_idx];
    const int direction = cobra.facing == LeftOrRight::Left ? -1 : 1;
    const Vec2 spit_origin = common::GetEmitPointForEntity(cobra, graphics, cobra.GetCenter());

    Entity* const spit = SpawnCobraSpitEntity(state, spit_origin);
    if (spit == nullptr) {
        return;
    }

    spit->facing = cobra.facing;
    spit->vel = Vec2::New(
        static_cast<float>(direction) * kCobraSpitVelocityX,
        kCobraSpitVelocityY
    );
    spit->acc = Vec2::New(0.0F, 0.0F);
    spit->thrown_by = cobra.vid;
    spit->thrown_immunity_timer = common::kThrownByImmunityDuration;
    spit->projectile_contact_damage_type = DamageType::Attack;
    spit->projectile_contact_damage_amount = kCobraVenomDamage;
    spit->projectile_contact_timer = common::kProjectileContactDuration;
    spit->counter_a = kCobraSpitLifetimeFrames;
    spit->counter_b = 0.0F;

    (void)PlayWorldSoundEmitter(state, spit_origin, audio_asset_ids::Tube);
    SpawnSpitSpray(state, spit_origin, direction);
}

void DestroyCobraSpit(std::size_t entity_idx, State& state) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& spit = state.entity_manager.entities[entity_idx];
    if (!spit.active) {
        return;
    }

    SpawnSpitImpact(state, spit.GetCenter());
    state.entity_manager.SetInactive(entity_idx);
}

} // namespace

void StepEntityLogicAsCobra(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Entity& cobra = state.entity_manager.entities[entity_idx];
    if (cobra.last_condition == EntityCondition::Stunned && cobra.condition == EntityCondition::Normal) {
        StartIdle(cobra);
    }
    if (cobra.condition != EntityCondition::Normal) {
        return;
    }

    if (cobra.counter_b > 0.0F) {
        cobra.counter_b -= 1.0F;
    }

    if (ShouldRunSightScan(cobra, state.stage_frame) && cobra.counter_b <= 0.0F &&
        CanSeePlayerAhead(cobra, state, graphics)) {
        if (state.player_vid.has_value()) {
            if (const Entity* const player = state.entity_manager.GetEntity(*state.player_vid)) {
                FaceTowards(cobra, player->GetCenter(), state.stage);
            }
        }
        cobra.vel.x = 0.0F;
        TrySetAnimation(cobra, EntityDisplayState::Walk);
        FireCobraSpit(entity_idx, state, graphics);
        cobra.counter_b = static_cast<float>(rng::RandomIntInclusive(
            kCobraSpitCooldownMinFrames,
            kCobraSpitCooldownMaxFrames
        ));
        StartIdle(cobra, 12, 20);
        return;
    }

    if (cobra.ai_state == EntityAiState::Idle) {
        cobra.vel.x = 0.0F;
        TrySetAnimation(cobra, EntityDisplayState::Neutral);
        if (cobra.counter_a > 0.0F) {
            cobra.counter_a -= 1.0F;
            return;
        }

        cobra.facing = rng::RandomIntInclusive(0, 1) == 0 ? LeftOrRight::Left : LeftOrRight::Right;
        StartWalking(cobra);
        return;
    }

    int direction = cobra.facing == LeftOrRight::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(cobra, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(cobra, state, graphics, direction)) {
        cobra.facing = cobra.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
        direction = -direction;
    }

    if (rng::RandomIntInclusive(1, kCobraIdleChance) == 1) {
        StartIdle(cobra);
        return;
    }

    cobra.vel.x = static_cast<float>(direction) * kCobraWalkSpeed;
    SetMovementFlag(cobra, EntityMovementFlag::Walking, true);
    TrySetAnimation(cobra, EntityDisplayState::Walk);
}

void StepEntityLogicAsCobraSpit(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& spit = state.entity_manager.entities[entity_idx];
    if (spit.counter_a > 0.0F) {
        spit.counter_a -= 1.0F;
    }
    if (spit.counter_a <= 0.0F) {
        DestroyCobraSpit(entity_idx, state);
        return;
    }

    spit.counter_b -= 1.0F;
    if (spit.counter_b <= 0.0F) {
        SpawnSpitTrail(state, spit.GetCenter(), spit.vel);
        spit.counter_b = kCobraSpitTrailIntervalFrames;
    }
}

void StepEntityPhysicsAsCobraSpit(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    entities::common::StepStandardPhysics(entity_idx, state, graphics, audio, dt);
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& spit = state.entity_manager.entities[entity_idx];
    if (!spit.active) {
        return;
    }
    if (spit.collided) {
        DestroyCobraSpit(entity_idx, state);
    }
}

extern const EntityArchetype kCobraArchetype{
    .type_ = EntityType::Cobra,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kCobraIdleMinFrames),
    .counter_b = static_cast<float>(kCobraSpitCooldownMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntityLogicAsCobra,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Cobra),
};

extern const EntityArchetype kCobraSpitArchetype{
    .type_ = EntityType::CobraSpit,
    .size = Vec2::New(4.0F, 3.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .collide_sound = audio_asset_ids::Tube,
    .step_logic = StepEntityLogicAsCobraSpit,
    .step_physics = StepEntityPhysicsAsCobraSpit,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::CobraSpit),
};

} // namespace splonks::entities::cobra

#include "ents/cobra.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/common/ground_walker.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <memory>
#include <optional>

namespace splonks::ents::cobra {

namespace {

constexpr float kCobraWalkSpeed = 0.85F;
constexpr float kCobraWalkAcceleration = 0.18F;
constexpr int kCobraIdleMinFrames = 18;
constexpr int kCobraIdleMaxFrames = 42;
constexpr int kCobraIdleChance = 120;
constexpr int kCobraSightVerticalTolerance = 18;
constexpr int kCobraSightDistance = 120;
constexpr std::uint64_t kCobraSightScanIntervalFrames = 12;
constexpr float kCobraSpitVelocityX = 2.2F;
constexpr float kCobraSpitVelocityY = -1.9F;
constexpr int kCobraSpitCooldownMinFrames = 48;
constexpr int kCobraSpitCooldownMaxFrames = 72;
constexpr float kCobraSpitLifetimeFrames = 120.0F;
constexpr float kCobraSpitTrailIntervalFrames = 2.0F;
constexpr std::uint32_t kCobraVenomDamage = 1;

void StartIdle(
    Ent& cobra,
    State& state,
    int min_frames = kCobraIdleMinFrames,
    int max_frames = kCobraIdleMaxFrames
) {
    cobra.ai_state = EntAiState::Idle;
    cobra.counter_a = static_cast<float>(state.drng.RandomIntInclusive(min_frames, max_frames));
    common::DecelerateHorizontallyToStop(cobra, kCobraWalkAcceleration);
    TrySetAnim(cobra, EntDisplayState::Neutral);
}

void StartWalking(Ent& cobra, const State& state) {
    cobra.ai_state = EntAiState::Patrolling;
    common::AccelerateHorizontallyTowardSpeed(
        cobra,
        state,
        cobra.facing == Side::Left ? -kCobraWalkSpeed : kCobraWalkSpeed,
        kCobraWalkAcceleration
    );
    TrySetAnim(cobra, EntDisplayState::Walk);
}

void FaceTowards(Ent& cobra, sim::Vec2 target_pos, const Stage& stage) {
    const sim::Vec2 delta = GetNearestWorldDelta(stage, cobra.GetSimCenter(), target_pos);
    if (delta.x < sim::Scalar::zero()) {
        cobra.facing = Side::Left;
    } else if (delta.x > sim::Scalar::zero()) {
        cobra.facing = Side::Right;
    }
}

bool ShouldRunSightScan(const Ent& cobra, std::uint64_t stage_frame) {
    return ((stage_frame + static_cast<std::uint64_t>(cobra.vid.id)) %
            kCobraSightScanIntervalFrames) == 0;
}

bool CanSeePlayerAhead(const Ent& cobra, const State& state, const Graphics& graphics) {
    const Vec2 spit_origin = common::GetEmitPointForEnt(cobra, graphics, cobra.GetRenderCenter());
    const sim::Vec2 sim_spit_origin = sim::ToSimVec2(spit_origin);
    const int direction = cobra.facing == Side::Left ? -1 : 1;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition != EntCondition::Normal) {
            continue;
        }
        const sim::Vec2 player_center =
            GetNearestWorldPoint(state.stage, sim_spit_origin, player->GetSimCenter());
        const sim::Vec2 player_delta = player_center - sim_spit_origin;
        if (player_delta.y.abs() > sim::Scalar::from_int(kCobraSightVerticalTolerance) ||
            player_delta.x.abs() > sim::Scalar::from_int(kCobraSightDistance)) {
            continue;
        }
        if ((direction < 0 && player_delta.x >= sim::Scalar::zero()) ||
            (direction > 0 && player_delta.x <= sim::Scalar::zero())) {
            continue;
        }
        const WorldRayHit hit = RaycastHorizontal(
            cobra,
            sim_spit_origin,
            direction,
            player_delta.x.abs().trunc_int(),
            state,
            graphics,
            cobra.vid
        );
        if (hit.type == WorldRayHitType::Ent && hit.ent_vid.has_value() &&
            *hit.ent_vid == player->vid) {
            return true;
        }
    }
    return false;
}

void SpawnSpitParticle(State& state, const Vec2& pos, const Vec2& vel, float alpha, float size_jitter) {
    SpriteParticle particle{};
    particle.aframe_animator = AFrameAnimator::New(aframe_ids::CobraSpit);
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

void FireCobraSpit(std::size_t ent_idx, State& state, Graphics& graphics) {
    Ent& cobra = state.ents.ents[ent_idx];
    const int direction = cobra.facing == Side::Left ? -1 : 1;
    const Vec2 spit_origin = common::GetEmitPointForEnt(cobra, graphics, cobra.GetRenderCenter());

    Ent* const spit = world_ops::SpawnEnt(state, EntType::CobraSpit, [&](Ent& spawned_spit) {
        spawned_spit.SetRenderCenter(spit_origin);
        spawned_spit.facing = cobra.facing;
        spawned_spit.vel = sim::ToSimVec2(Vec2::New(
            static_cast<float>(direction) * kCobraSpitVelocityX,
            kCobraSpitVelocityY
        ));
        spawned_spit.acc = sim::Vec2::zero();
        spawned_spit.thrown_by = cobra.vid;
        spawned_spit.thrown_immunity_timer = common::kThrownByImmunityDuration;
        spawned_spit.proj_contact_damage_type = DamageType::Attack;
        spawned_spit.proj_contact_damage_amount = kCobraVenomDamage;
        spawned_spit.proj_contact_timer = common::kProjContactDuration;
        spawned_spit.counter_a = kCobraSpitLifetimeFrames;
        spawned_spit.counter_b = 0.0F;
    });
    if (spit == nullptr) {
        return;
    }

    (void)PlayWorldSoundEmitter(state, spit_origin, audio_asset_ids::Tube);
    SpawnSpitSpray(state, spit_origin, direction);
}

void DestroyCobraSpit(std::size_t ent_idx, State& state) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& spit = state.ents.ents[ent_idx];
    if (!spit.active) {
        return;
    }

    SpawnSpitImpact(state, spit.GetRenderCenter());
    (void)world_ops::DeactivateEnt(state, spit.vid);
}

} // namespace

void StepEntLogicAsCobra(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Ent& cobra = state.ents.ents[ent_idx];
    if (cobra.last_condition == EntCondition::Stunned && cobra.condition == EntCondition::Normal) {
        StartIdle(cobra, state);
    }
    if (cobra.condition != EntCondition::Normal) {
        return;
    }

    if (cobra.counter_b > 0.0F) {
        cobra.counter_b -= 1.0F;
    }

    if (ShouldRunSightScan(cobra, state.stage_frame) && cobra.counter_b <= 0.0F &&
        CanSeePlayerAhead(cobra, state, graphics)) {
        if (const Ent* const player = FindNearestPlayer(state, cobra.GetSimCenter())) {
            FaceTowards(cobra, player->GetSimCenter(), state.stage);
        }
        common::DecelerateHorizontallyToStop(cobra, kCobraWalkAcceleration);
        TrySetAnim(cobra, EntDisplayState::Walk);
        FireCobraSpit(ent_idx, state, graphics);
        cobra.counter_b = static_cast<float>(state.drng.RandomIntInclusive(
            kCobraSpitCooldownMinFrames,
            kCobraSpitCooldownMaxFrames
        ));
        StartIdle(cobra, state, 12, 20);
        return;
    }

    if (cobra.ai_state == EntAiState::Idle) {
        common::DecelerateHorizontallyToStop(cobra, kCobraWalkAcceleration);
        TrySetAnim(cobra, EntDisplayState::Neutral);
        if (cobra.counter_a > 0.0F) {
            cobra.counter_a -= 1.0F;
            return;
        }

        cobra.facing =
            state.drng.RandomIntInclusive(0, 1) == 0 ? Side::Left : Side::Right;
        StartWalking(cobra, state);
        return;
    }

    int direction = cobra.facing == Side::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(cobra, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(cobra, state, graphics, direction)) {
        cobra.facing = cobra.facing == Side::Left ? Side::Right : Side::Left;
        direction = -direction;
    }

    if (state.drng.RandomIntInclusive(1, kCobraIdleChance) == 1) {
        StartIdle(cobra, state);
        return;
    }

    common::AccelerateHorizontallyTowardSpeed(
        cobra,
        state,
        static_cast<float>(direction) * kCobraWalkSpeed,
        kCobraWalkAcceleration
    );
    SetMovementFlag(cobra, EntMovementFlag::Walking, true);
    TrySetAnim(cobra, EntDisplayState::Walk);
}

void StepEntLogicAsCobraSpit(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Ent& spit = state.ents.ents[ent_idx];
    if (spit.counter_a > 0.0F) {
        spit.counter_a -= 1.0F;
    }
    if (spit.counter_a <= 0.0F) {
        DestroyCobraSpit(ent_idx, state);
        return;
    }

    spit.counter_b -= 1.0F;
    if (spit.counter_b <= 0.0F) {
        SpawnSpitTrail(state, spit.GetRenderCenter(), spit.GetRenderVel());
        spit.counter_b = kCobraSpitTrailIntervalFrames;
    }
}

void StepEntPhysicsAsCobraSpit(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    ents::common::StepStandardPhysics(ent_idx, state, graphics, audio, dt);
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& spit = state.ents.ents[ent_idx];
    if (!spit.active) {
        return;
    }
    if (spit.collided) {
        DestroyCobraSpit(ent_idx, state);
    }
}

extern const EntSpec kCobraSpec{
    .type_ = EntType::Cobra,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
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
    .counter_a = EntSpecCounter(static_cast<float>(kCobraIdleMinFrames)),
    .counter_b = EntSpecCounter(static_cast<float>(kCobraSpitCooldownMinFrames)),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntLogicAsCobra,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Cobra),
};

extern const EntSpec kCobraSpitSpec{
    .type_ = EntType::CobraSpit,
    .size = EntSpecSize(4.0F, 3.0F),
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
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .collide_sound = audio_asset_ids::Tube,
    .step_logic = StepEntLogicAsCobraSpit,
    .step_physics = StepEntPhysicsAsCobraSpit,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::CobraSpit),
};

} // namespace splonks::ents::cobra

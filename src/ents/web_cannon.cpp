#include "ents/web_cannon.hpp"

#include "audio.hpp"
#include "controls.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/common/discrete_aim.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "particles/sprite_particle.hpp"
#include "fxp.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>

namespace splonks::ents::web_cannon {

namespace {

constexpr float kWebGunFireCooldownFrames = 10.0F;
constexpr float kWebGunReloadCooldownFrames = 42.0F;
constexpr float kWebGunBurstShots = 3.0F;
constexpr float kWebBallSpeedX = 6.5F;
constexpr float kWebBallLifetimeFrames = 110.0F;
constexpr float kWebBallEntArmDelayFrames = 2.0F;
constexpr float kWebBallTrailIntervalFrames = 3.0F;
constexpr float kTemporaryCobwebLifetimeFrames = 540.0F;
constexpr float kCobwebWearIntervalFrames = 6.0F;
constexpr std::uint32_t kCobwebDurability = 8;
constexpr float kCobwebHorizontalDamping = 0.18F;
constexpr float kCobwebVerticalDamping = 0.25F;
constexpr float kCobwebAccelerationDamping = 0.0F;
constexpr float kCobwebJumpEscapeVelocity = -1.7F;
constexpr float kCobwebOccupantSpeedThreshold = 0.05F;
constexpr float kCobwebOccupantSpeedThresholdSq =
    kCobwebOccupantSpeedThreshold * kCobwebOccupantSpeedThreshold;

using WebGunAim = common::DiscreteHeldWeaponAim;

WebGunAim GetWebGunAim(const Ent& weapon, const Ent* holder, const State& state) {
    return common::GetDiscreteHeldWeaponAim(weapon, holder, state);
}

IVec2 SnapWorldPointToTile(FxVec2 point, const Stage& stage) {
    const int tile_size = static_cast<int>(kTileSize);
    const int tile_x = FloorDiv(point.x.floor_int(), tile_size);
    const int tile_y = FloorDiv(point.y.floor_int(), tile_size);
    return stage.WrapTileCoord(IVec2::New(tile_x, tile_y));
}

FxVec2 TileCenterToWorld(const IVec2& tile_pos) {
    const int tile_size = static_cast<int>(kTileSize);
    return PixelVec2(tile_pos.x * tile_size + 8, tile_pos.y * tile_size + 8);
}

bool HasCobwebAtTile(const IVec2& tile_pos, const State& state) {
    const IVec2 wrapped_tile_pos = state.stage.WrapTileCoord(tile_pos);
    for (const Ent& ent : state.ents.ents) {
        if (!ent.active || ent.type_ != EntType::Cobweb) {
            continue;
        }
        if (SnapWorldPointToTile(ent.GetCenter(), state.stage) == wrapped_tile_pos) {
            return true;
        }
    }
    return false;
}

bool CanSpawnCobwebAtTile(const IVec2& tile_pos, const State& state) {
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtTilePos(state.stage, tile_pos);
    if (!tile_query.has_value() || tile_query->tile == nullptr) {
        return false;
    }
    return !IsTileCollidable(*tile_query->tile) && !HasCobwebAtTile(tile_query->tile_pos, state);
}

bool IsWorldPointInsideSolidTile(FxVec2 point, const State& state) {
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, point);
    return tile_query.has_value() && tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile);
}

std::optional<IVec2> GetCobwebGrowthTile(const Ent& web_ball, const Ent& hit_cobweb, const State& state) {
    const IVec2 hit_tile = SnapWorldPointToTile(hit_cobweb.GetCenter(), state.stage);
    std::array<IVec2, 4> candidates{};
    std::size_t candidate_count = 0;

    auto push_candidate = [&](const IVec2& tile_pos) {
        const IVec2 wrapped = state.stage.WrapTileCoord(tile_pos);
        if (wrapped == hit_tile) {
            return;
        }
        for (std::size_t i = 0; i < candidate_count; ++i) {
            if (candidates[i] == wrapped) {
                return;
            }
        }
        if (candidate_count < candidates.size()) {
            candidates[candidate_count++] = wrapped;
        }
    };

    push_candidate(SnapWorldPointToTile(web_ball.GetCenter(), state.stage));

    FxVec2 incoming_dir = web_ball.vel * FxScalar::from_int(-1);
    if (incoming_dir == FxVec2::zero()) {
        incoming_dir = web_ball.facing == Side::Left
            ? FxVec2{FxScalar::from_int(1), FxScalar::zero()}
            : FxVec2{FxScalar::from_int(-1), FxScalar::zero()};
    }

    const int step_x = incoming_dir.x > FxScalar::zero()
        ? 1
        : (incoming_dir.x < FxScalar::zero() ? -1 : 0);
    const int step_y = incoming_dir.y > FxScalar::zero()
        ? 1
        : (incoming_dir.y < FxScalar::zero() ? -1 : 0);
    const bool x_dominant = incoming_dir.x.abs() >= incoming_dir.y.abs();

    if (x_dominant) {
        if (step_x != 0) {
            push_candidate(hit_tile + IVec2::New(step_x, 0));
        }
        if (step_y != 0) {
            push_candidate(hit_tile + IVec2::New(0, step_y));
        }
    } else {
        if (step_y != 0) {
            push_candidate(hit_tile + IVec2::New(0, step_y));
        }
        if (step_x != 0) {
            push_candidate(hit_tile + IVec2::New(step_x, 0));
        }
    }

    for (std::size_t i = 0; i < candidate_count; ++i) {
        if (CanSpawnCobwebAtTile(candidates[i], state)) {
            return candidates[i];
        }
    }
    return std::nullopt;
}

bool CanBeAffectedByCobweb(const Ent& ent) {
    return ent.active && ent.affected_by_cobweb && !ent.held_by_vid.has_value();
}

void SpawnWebParticle(
    State& state,
    const FVec2& pos,
    const FVec2& vel,
    float alpha,
    float size,
    float lifetime
) {
    SpriteParticle particle{};
    particle.aframe_animator = AFrameAnimator::New(aframe_ids::WebBall);
    particle.draw_layer = DrawLayer::Foreground;
    particle.counter = static_cast<std::uint32_t>(std::max(1.0F, lifetime));
    particle.pos = pos + FVec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
    particle.size = FVec2::New(size, size);
    particle.rot = rng::RandomFloat(-18.0F, 18.0F);
    particle.alpha = alpha;
    particle.vel = vel;
    particle.svel = FVec2::New(0.02F, 0.02F);
    particle.rotvel = rng::RandomFloat(-1.2F, 1.2F);
    particle.alpha_vel = -0.05F;
    particle.acc = FVec2::New(0.0F, 0.08F);
    particle.alpha_acc = -0.002F;
    state.particles.Add(std::move(particle));
}

void SpawnWebSpray(State& state, const FVec2& origin, const FVec2& direction) {
    const FVec2 normalized_direction = NormalizeOrZeroDeterministic(direction);
    for (int i = 0; i < 7; ++i) {
        const FVec2 spray_velocity =
            (normalized_direction * rng::RandomFloat(0.35F, 1.5F)) +
            FVec2::New(rng::RandomFloat(-0.45F, 0.45F), rng::RandomFloat(-0.45F, 0.45F));
        SpawnWebParticle(
            state,
            origin,
            spray_velocity,
            rng::RandomFloat(0.7F, 0.95F),
            rng::RandomFloat(4.0F, 6.0F),
            rng::RandomFloat(8.0F, 14.0F)
        );
    }
}

void SpawnWebTrail(State& state, const FVec2& origin, const FVec2& base_vel) {
    for (int i = 0; i < 2; ++i) {
        SpawnWebParticle(
            state,
            origin,
            FVec2::New(
                base_vel.x * rng::RandomFloat(-0.10F, 0.04F),
                base_vel.y * rng::RandomFloat(-0.10F, 0.04F)
            ) + FVec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.08F, 0.08F)),
            rng::RandomFloat(0.35F, 0.6F),
            rng::RandomFloat(3.5F, 5.0F),
            rng::RandomFloat(6.0F, 10.0F)
        );
    }
}

void SpawnCobwebBurst(State& state, const FVec2& origin) {
    for (int i = 0; i < 6; ++i) {
        SpawnWebParticle(
            state,
            origin,
            FVec2::New(rng::RandomFloat(-0.9F, 0.9F), rng::RandomFloat(-1.0F, 0.15F)),
            rng::RandomFloat(0.65F, 0.9F),
            rng::RandomFloat(4.0F, 6.0F),
            rng::RandomFloat(8.0F, 14.0F)
        );
    }
}

Ent* SpawnCobwebEnt(State& state, FxVec2 center, bool temporary) {
    return world_ops::SpawnEnt(state, EntType::Cobweb, [&](Ent& cobweb) {
        cobweb.SetCenter(center);
        cobweb.counter_a = temporary ? ToFxScalar(kTemporaryCobwebLifetimeFrames)
                                     : FxScalar::zero();
        cobweb.counter_d = ToFxScalar(kCobwebWearIntervalFrames);
        cobweb.health = kCobwebDurability;
    });
}

void DestroyCobweb(std::size_t ent_idx, State& state) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& cobweb = state.ents.ents[ent_idx];
    if (!cobweb.active) {
        return;
    }

    SpawnCobwebBurst(state, ToFVec2(cobweb.GetCenter()));
    (void)world_ops::DeactivateEnt(state, cobweb.vid);
}

void TriggerWebBallBurst(std::size_t ent_idx, State& state, bool spawn_cobweb) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& web_ball = state.ents.ents[ent_idx];
    if (!web_ball.active) {
        return;
    }

    const FxVec2 impact_center = web_ball.GetCenter();
    if (spawn_cobweb) {
        const IVec2 tile_pos = SnapWorldPointToTile(impact_center, state.stage);
        if (CanSpawnCobwebAtTile(tile_pos, state)) {
            (void)SpawnCobwebEnt(state, TileCenterToWorld(tile_pos), true);
        }
    }
    SpawnCobwebBurst(state, ToFVec2(impact_center));
    (void)world_ops::DeactivateEnt(state, web_ball.vid);
}

void TriggerWebBallBurstAtTile(std::size_t ent_idx, State& state, const IVec2& tile_pos) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& web_ball = state.ents.ents[ent_idx];
    if (!web_ball.active) {
        return;
    }

    const FxVec2 impact_center = web_ball.GetCenter();
    if (CanSpawnCobwebAtTile(tile_pos, state)) {
        (void)SpawnCobwebEnt(state, TileCenterToWorld(tile_pos), true);
    }
    SpawnCobwebBurst(state, ToFVec2(impact_center));
    (void)world_ops::DeactivateEnt(state, web_ball.vid);
}

void FireWebGun(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Ent& weapon = state.ents.ents[ent_idx];
    Ent* holder = nullptr;
    if (weapon.held_by_vid.has_value()) {
        holder = state.ents.GetEntMut(*weapon.held_by_vid);
    }

    const WebGunAim aim = GetWebGunAim(weapon, holder, state);
    weapon.facing = aim.facing;
    weapon.rotation = aim.rotation;

    const FxVec2 muzzle_pos = weapon.GetCenter() + (aim.direction * FxScalar::from_int(8));
    const FxVec2 spawn_pos = muzzle_pos + (aim.direction * FxScalar::from_int(4));
    const FVec2 render_muzzle_pos = ToFVec2(muzzle_pos);

    if (holder != nullptr && IsWorldPointInsideSolidTile(muzzle_pos, state)) {
        const IVec2 holder_tile = SnapWorldPointToTile(holder->GetCenter(), state.stage);
        if (CanSpawnCobwebAtTile(holder_tile, state)) {
            (void)SpawnCobwebEnt(state, TileCenterToWorld(holder_tile), true);
        }
        const FVec2 holder_center = ToFVec2(holder->GetCenter());
        (void)PlayWorldSoundEmitter(state, holder_center, audio_asset_ids::PistolShoot);
        SpawnWebSpray(state, holder_center, ToFVec2(aim.direction));
        if (holder != nullptr) {
            holder->vel -= aim.direction * ToFxScalar(0.12F);
        }
        return;
    }

    (void)world_ops::SpawnEnt(state, EntType::WebBall, [&](Ent& spawned_web_ball) {
        spawned_web_ball.SetCenter(spawn_pos);
        spawned_web_ball.facing = aim.facing;
        spawned_web_ball.vel = aim.direction * ToFxScalar(kWebBallSpeedX) +
                               (holder != nullptr
                                    ? holder->vel * ToFxScalar(0.35F)
                                    : FxVec2::zero());
        spawned_web_ball.acc = FxVec2::zero();
        spawned_web_ball.thrown_by =
            holder != nullptr ? std::optional<VID>(holder->vid) : weapon.use_state.user_vid;
        spawned_web_ball.thrown_immunity_timer = common::kThrownByImmunityDuration;
        spawned_web_ball.counter_a = ToFxScalar(kWebBallLifetimeFrames);
        spawned_web_ball.counter_b = FxScalar::zero();
        spawned_web_ball.counter_c = ToFxScalar(kWebBallEntArmDelayFrames);
    });

    (void)PlayWorldSoundEmitter(state, render_muzzle_pos, audio_asset_ids::PistolShoot);
    SpawnWebSpray(state, render_muzzle_pos, ToFVec2(aim.direction));
    if (holder != nullptr) {
        holder->vel -= aim.direction * ToFxScalar(0.12F);
    }
}

bool ApplyCobwebToEnt(std::size_t cobweb_idx, Ent& other, State& state) {
    if (cobweb_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& cobweb = state.ents.ents[cobweb_idx];
    if (!cobweb.active || !CanBeAffectedByCobweb(other)) {
        return false;
    }

    other.vel.x *= ToFxScalar(kCobwebHorizontalDamping);
    other.vel.y *= ToFxScalar(kCobwebVerticalDamping);
    other.acc.x *= ToFxScalar(kCobwebAccelerationDamping);
    other.acc.y *= ToFxScalar(kCobwebAccelerationDamping);
    if (other.vel.x.abs() < ToFxScalar(kCobwebOccupantSpeedThreshold)) {
        other.vel.x = FxScalar::zero();
    }
    if (other.vel.y.abs() < ToFxScalar(kCobwebOccupantSpeedThreshold)) {
        other.vel.y = FxScalar::zero();
    }
    other.fall_timer = 0;

    if (other.proj_contact_timer > 0) {
        other.proj_contact_timer = 0;
        const EntSpec& spec = GetEntSpec(other.type_);
        other.proj_contact_damage_type = spec.proj_contact_damage_type;
        other.proj_contact_damage_amount = spec.proj_contact_damage_amount;
    }

    const controls::ControlIntent intent = controls::GetControlIntentForEnt(other, state);
    if (intent.jump_pressed && other.vel.y > ToFxScalar(kCobwebJumpEscapeVelocity)) {
        other.vel.y = ToFxScalar(kCobwebJumpEscapeVelocity);
    }

    return true;
}

common::ContactResult OnEntContactAsWebBall(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    if (context.phase != common::ContactPhase::SweptEntered ||
        !context.mover_vid.has_value() ||
        *context.mover_vid != state.ents.ents[ent_idx].vid) {
        return common::ContactResult{};
    }
    if (ent_idx >= state.ents.ents.size() || other_ent_idx >= state.ents.ents.size()) {
        return common::ContactResult{};
    }

    const Ent& web_ball = state.ents.ents[ent_idx];
    const Ent& other = state.ents.ents[other_ent_idx];
    if (!web_ball.active || !other.active) {
        return common::ContactResult{};
    }
    if (web_ball.counter_c > FxScalar::zero()) {
        return common::ContactResult{};
    }
    if (web_ball.thrown_by.has_value() && other.vid == *web_ball.thrown_by) {
        return common::ContactResult{};
    }
    if (web_ball.thrown_by.has_value() && other.held_by_vid.has_value() &&
        *other.held_by_vid == *web_ball.thrown_by) {
        return common::ContactResult{};
    }

    if (audio != nullptr) {
        switch (other.type_) {
        case EntType::Pot:
        case EntType::Box:
        case EntType::Skull:
            (void)common::TryDamageEnt(other_ent_idx, state, *audio, DamageType::Attack, 1);
            break;
        default:
            break;
        }
    }

    if (other.type_ != EntType::Cobweb) {
        if (Ent* const other_mut = state.ents.GetEntMut(other.vid)) {
            other_mut->vel = FxVec2::zero();
            other_mut->acc = FxVec2::zero();
            other_mut->fall_timer = 0;
        }
        TriggerWebBallBurst(ent_idx, state, true);
        return common::ContactResult{.stop_sweep = true};
    }

    if (const std::optional<IVec2> growth_tile = GetCobwebGrowthTile(web_ball, other, state); growth_tile.has_value()) {
        TriggerWebBallBurstAtTile(ent_idx, state, *growth_tile);
    } else {
        TriggerWebBallBurst(ent_idx, state, false);
    }
    return common::ContactResult{.stop_sweep = true};
}

common::ContactResult OnTileContactAsWebBall(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    if (context.phase != common::ContactPhase::AttemptedBlocked) {
        return common::ContactResult{};
    }

    TriggerWebBallBurst(ent_idx, state, true);
    return common::ContactResult{.stop_sweep = true};
}

common::ContactResult OnEntContactAsCobweb(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)audio;
    if (graphics == nullptr || context.phase != common::ContactPhase::SweptEntered) {
        return common::ContactResult{};
    }
    if (ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return common::ContactResult{};
    }

    Ent& cobweb = state.ents.ents[ent_idx];
    Ent& other = state.ents.ents[other_ent_idx];
    if (!cobweb.active || cobweb.type_ != EntType::Cobweb || !CanBeAffectedByCobweb(other)) {
        return common::ContactResult{};
    }
    if (!context.mover_vid.has_value() || *context.mover_vid != other.vid) {
        return common::ContactResult{};
    }

    const FxAABB cobweb_aabb = common::GetContactAabbForEnt(cobweb, *graphics);
    const FxAABB other_aabb = GetNearestWorldAabb(
        state.stage,
        cobweb_aabb.center(),
        common::GetContactAabbForEnt(other, *graphics)
    );
    if (!gfxp::aabbs_intersect(cobweb_aabb, other_aabb)) {
        return common::ContactResult{};
    }

    const bool applied = ApplyCobwebToEnt(ent_idx, other, state);
    if (!applied) {
        return common::ContactResult{};
    }

    const bool is_controlled = state.controlled_ent_vid.has_value() &&
                               other.vid == *state.controlled_ent_vid;
    const bool is_player = IsPlayerLikeEntType(other.type_);
    if (is_controlled || is_player) {
        return common::ContactResult{};
    }

    other.grounded = true;
    return common::ContactResult{.stop_sweep = true};
}

EntDamageEffectResult OnDamageAsCobweb(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    (void)audio;
    (void)amount;
    (void)damage_applied;
    if (ent_idx >= state.ents.ents.size()) {
        return EntDamageEffectResult::None;
    }

    Ent& cobweb = state.ents.ents[ent_idx];
    if (!cobweb.active || cobweb.type_ != EntType::Cobweb) {
        return EntDamageEffectResult::None;
    }

    switch (damage_type) {
    case DamageType::JumpOn:
        return EntDamageEffectResult::None;
    case DamageType::Attack:
        if (amount >= cobweb.health) {
            cobweb.health = 0;
            DestroyCobweb(ent_idx, state);
        } else {
            cobweb.health -= amount;
        }
        return EntDamageEffectResult::Consumed;
    case DamageType::IgnitingAttack:
    case DamageType::Burn:
    case DamageType::Explosion:
    case DamageType::Crush:
        cobweb.health = 0;
        DestroyCobweb(ent_idx, state);
        return EntDamageEffectResult::Consumed;
    default:
        return EntDamageEffectResult::None;
    }
}

} // namespace

void OnUseAsWebCannon(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    Ent& weapon = state.ents.ents[ent_idx];
    if (!weapon.use_state.pressed || weapon.counter_a > FxScalar::zero()) {
        return;
    }

    if (weapon.counter_b <= FxScalar::zero()) {
        weapon.counter_b = ToFxScalar(kWebGunBurstShots);
    }

    FireWebGun(ent_idx, state, graphics, audio);
    weapon.counter_b -= FxScalar::from_int(1);
    weapon.counter_a = weapon.counter_b <= FxScalar::zero()
        ? ToFxScalar(kWebGunReloadCooldownFrames)
        : ToFxScalar(kWebGunFireCooldownFrames);

    if (weapon.use_state.source == AttachMode::None) {
        StopUsingEnt(weapon);
    }
}

void StepEntLogicAsWebCannon(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Ent& weapon = state.ents.ents[ent_idx];
    if (weapon.counter_a > FxScalar::zero()) {
        weapon.counter_a -= FxScalar::from_int(1);
        if (weapon.counter_a < FxScalar::zero()) {
            weapon.counter_a = FxScalar::zero();
        }
    }

    const Ent* holder = nullptr;
    if (weapon.held_by_vid.has_value()) {
        holder = state.ents.GetEnt(*weapon.held_by_vid);
    }
    if (holder == nullptr) {
        weapon.rotation = FxScalar::zero();
        return;
    }

    const WebGunAim aim = GetWebGunAim(weapon, holder, state);
    weapon.facing = aim.facing;
    weapon.rotation = aim.rotation;
}

void StepEntLogicAsWebBall(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Ent& web_ball = state.ents.ents[ent_idx];
    if (web_ball.counter_a > FxScalar::zero()) {
        web_ball.counter_a -= FxScalar::from_int(1);
    }
    if (web_ball.counter_a <= FxScalar::zero()) {
        TriggerWebBallBurst(ent_idx, state, true);
        return;
    }
    if (web_ball.counter_c > FxScalar::zero()) {
        web_ball.counter_c -= FxScalar::from_int(1);
        if (web_ball.counter_c < FxScalar::zero()) {
            web_ball.counter_c = FxScalar::zero();
        }
    }

    web_ball.counter_b -= FxScalar::from_int(1);
    if (web_ball.counter_b <= FxScalar::zero()) {
        SpawnWebTrail(state, ToFVec2(web_ball.GetCenter()), ToFVec2(web_ball.vel));
        web_ball.counter_b = ToFxScalar(kWebBallTrailIntervalFrames);
    }
}

void StepEntPhysicsAsWebBall(
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

    Ent& web_ball = state.ents.ents[ent_idx];
    if (!web_ball.active) {
        return;
    }
    if (web_ball.collided) {
        TriggerWebBallBurst(ent_idx, state, true);
    }
}

void StepEntLogicAsCobweb(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& cobweb = state.ents.ents[ent_idx];
    if (!cobweb.active || cobweb.type_ != EntType::Cobweb) {
        return;
    }

    if (cobweb.counter_a > FxScalar::zero()) {
        cobweb.counter_a -= FxScalar::from_int(1);
        if (cobweb.counter_a <= FxScalar::zero()) {
            DestroyCobweb(ent_idx, state);
            return;
        }
    }

    const float health_ratio = static_cast<float>(cobweb.health) / static_cast<float>(std::max<std::uint32_t>(1, kCobwebDurability));
    const float lifetime_ratio = cobweb.counter_a > FxScalar::zero()
        ? std::clamp(
              ToFloat(cobweb.counter_a) / kTemporaryCobwebLifetimeFrames,
              0.0F,
              1.0F
          )
        : 1.0F;
    cobweb.alpha = ToFxScalar(std::clamp(std::min(health_ratio, lifetime_ratio), 0.0F, 1.0F));

    const FxAABB cobweb_aabb = common::GetContactAabbForEnt(cobweb, graphics);
    const std::vector<VID> overlapped_vids = QueryEntsInAabb(state, cobweb_aabb, cobweb.vid);
    bool occupied = false;
    for (const VID& other_vid : overlapped_vids) {
        Ent* const other = state.ents.GetEntMut(other_vid);
        if (other == nullptr || !other->active) {
            continue;
        }

        const FxAABB other_aabb = GetNearestWorldAabb(
            state.stage,
            cobweb_aabb.center(),
            common::GetContactAabbForEnt(*other, graphics)
        );
        if (!gfxp::aabbs_intersect(cobweb_aabb, other_aabb)) {
            continue;
        }

        if (!CanBeAffectedByCobweb(*other)) {
            continue;
        }

        const controls::ControlIntent intent = controls::GetControlIntentForEnt(*other, state);
        const bool moving_in_web =
            gfxp::length_sq(other->vel) > ToFxScalar(kCobwebOccupantSpeedThresholdSq) ||
            gfxp::length_sq(other->acc) > FxScalar::zero() ||
                                   intent.jump_pressed;
        occupied = true;
        if (moving_in_web && cobweb.health > 0) {
            cobweb.counter_d -= FxScalar::from_int(1);
            if (cobweb.counter_d <= FxScalar::zero()) {
                cobweb.counter_d = ToFxScalar(kCobwebWearIntervalFrames);
                cobweb.health = std::max<std::uint32_t>(0, cobweb.health - 1);
                if (cobweb.health == 0) {
                    DestroyCobweb(ent_idx, state);
                    return;
                }
            }
        }
    }

    if (!occupied) {
        cobweb.counter_d = ToFxScalar(kCobwebWearIntervalFrames);
    }
}

extern const EntSpec kWebCannonSpec{
    .type_ = EntType::WebCannon,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .preserve_held_aim = true,
    .predict_local_attach_use = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_b = ToFxScalar(kWebGunBurstShots),
    .damage_vuln = DamageVuln::Vulnerable,
    .on_use = OnUseAsWebCannon,
    .step_logic = StepEntLogicAsWebCannon,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::WebCannon),
};

extern const EntSpec kWebBallSpec{
    .type_ = EntType::WebBall,
    .size = EntSpecSize(7.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = false,
    .can_receive_proj_contact = false,
    .can_be_picked_up = false,
    .affected_by_cobweb = false,
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
    .can_apply_proj_contact = false,
    .step_logic = StepEntLogicAsWebBall,
    .step_physics = StepEntPhysicsAsWebBall,
    .on_ent_contact = OnEntContactAsWebBall,
    .on_tile_contact = OnTileContactAsWebBall,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::WebBall),
};

extern const EntSpec kCobwebSpec{
    .type_ = EntType::Cobweb,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = kCobwebDurability,
    .has_physics = false,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_proj_contact = false,
    .can_be_picked_up = false,
    .affected_by_cobweb = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Background,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .can_apply_proj_contact = false,
    .on_damage = OnDamageAsCobweb,
    .step_logic = StepEntLogicAsCobweb,
    .on_ent_contact = OnEntContactAsCobweb,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Cobweb),
};

} // namespace splonks::ents::web_cannon

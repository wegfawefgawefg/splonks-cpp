#include "ents/web_cannon.hpp"

#include "audio.hpp"
#include "controls.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "particles/sprite_particle.hpp"
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
constexpr float kDiagonalAimComponent = 0.707106769F;

struct WebGunAim {
    Vec2 direction = Vec2::New(1.0F, 0.0F);
    Side facing = Side::Right;
    float rotation = 0.0F;
};

float NormalizeDegrees(float degrees) {
    while (degrees > 180.0F) {
        degrees -= 360.0F;
    }
    while (degrees <= -180.0F) {
        degrees += 360.0F;
    }
    return degrees;
}

Vec2 DiscreteAimDirection(int aim_x, int aim_y, Side facing) {
    if (aim_x == 0 && aim_y == 0) {
        return facing == Side::Left ? Vec2::New(-1.0F, 0.0F) : Vec2::New(1.0F, 0.0F);
    }
    if (aim_x != 0 && aim_y != 0) {
        return Vec2::New(
            static_cast<float>(aim_x) * kDiagonalAimComponent,
            static_cast<float>(aim_y) * kDiagonalAimComponent
        );
    }
    return Vec2::New(static_cast<float>(aim_x), static_cast<float>(aim_y));
}

float DiscreteAimWorldAngle(int aim_x, int aim_y, Side facing) {
    if (aim_x == 0 && aim_y == 0) {
        return facing == Side::Left ? 180.0F : 0.0F;
    }
    if (aim_x > 0) {
        if (aim_y < 0) {
            return -45.0F;
        }
        if (aim_y > 0) {
            return 45.0F;
        }
        return 0.0F;
    }
    if (aim_x < 0) {
        if (aim_y < 0) {
            return -135.0F;
        }
        if (aim_y > 0) {
            return 135.0F;
        }
        return 180.0F;
    }
    return aim_y < 0 ? -90.0F : 90.0F;
}

WebGunAim GetWebGunAim(const Ent& weapon, const Ent* holder, const State& state) {
    int aim_x = 0;
    int aim_y = 0;
    Side facing = holder != nullptr ? holder->facing : weapon.facing;
    if (holder != nullptr) {
        const controls::ControlIntent intent = controls::GetControlIntentForEnt(*holder, state);
        if (intent.left && !intent.right) {
            aim_x = -1;
        } else if (intent.right && !intent.left) {
            aim_x = 1;
        }
        if (intent.up && !intent.down) {
            aim_y = -1;
        } else if (intent.down && !intent.up) {
            aim_y = 1;
        }
    }

    if (aim_x < 0) {
        facing = Side::Left;
    } else if (aim_x > 0) {
        facing = Side::Right;
    }

    const Vec2 direction = DiscreteAimDirection(aim_x, aim_y, facing);
    const float world_angle = DiscreteAimWorldAngle(aim_x, aim_y, facing);
    const float base_angle = facing == Side::Left ? 180.0F : 0.0F;
    return WebGunAim{
        .direction = direction,
        .facing = facing,
        .rotation = NormalizeDegrees(world_angle - base_angle),
    };
}

IVec2 SnapWorldPointToTile(const Vec2& point, const Stage& stage) {
    const int tile_x = FloorToInt(point.x / static_cast<float>(kTileSize));
    const int tile_y = FloorToInt(point.y / static_cast<float>(kTileSize));
    return stage.WrapTileCoord(IVec2::New(tile_x, tile_y));
}

Vec2 TileCenterToWorld(const IVec2& tile_pos) {
    return Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
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

bool IsWorldPointInsideSolidTile(const Vec2& point, const State& state) {
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, ToIVec2(point));
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

    Vec2 incoming_dir = web_ball.vel * -1.0F;
    if (incoming_dir == Vec2::New(0.0F, 0.0F)) {
        incoming_dir = web_ball.facing == Side::Left ? Vec2::New(1.0F, 0.0F) : Vec2::New(-1.0F, 0.0F);
    }

    const int step_x = incoming_dir.x > 0.0F ? 1 : (incoming_dir.x < 0.0F ? -1 : 0);
    const int step_y = incoming_dir.y > 0.0F ? 1 : (incoming_dir.y < 0.0F ? -1 : 0);
    const bool x_dominant = std::abs(incoming_dir.x) >= std::abs(incoming_dir.y);

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

bool AabbOverlapsAnyCobweb(const AABB& target_aabb, VID self_vid, const State& state, const Graphics& graphics) {
    for (const VID& other_vid : QueryEntsInAabb(state, target_aabb, self_vid)) {
        const Ent* const other = state.ents.GetEnt(other_vid);
        if (other == nullptr || !other->active || other->type_ != EntType::Cobweb) {
            continue;
        }

        const AABB cobweb_aabb = GetNearestWorldAabb(
            state.stage,
            (target_aabb.tl + target_aabb.br) * 0.5F,
            common::GetContactAabbForEnt(*other, graphics)
        );
        if (AabbsIntersect(target_aabb, cobweb_aabb)) {
            return true;
        }
    }
    return false;
}

bool CanBeAffectedByCobweb(const Ent& ent) {
    return ent.active && ent.affected_by_cobweb && !ent.held_by_vid.has_value();
}

void SpawnWebParticle(
    State& state,
    const Vec2& pos,
    const Vec2& vel,
    float alpha,
    float size,
    float lifetime
) {
    SpriteParticle particle{};
    particle.aframe_animator = AFrameAnimator::New(aframe_ids::WebBall);
    particle.draw_layer = DrawLayer::Foreground;
    particle.counter = static_cast<std::uint32_t>(std::max(1.0F, lifetime));
    particle.pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
    particle.size = Vec2::New(size, size);
    particle.rot = rng::RandomFloat(-18.0F, 18.0F);
    particle.alpha = alpha;
    particle.vel = vel;
    particle.svel = Vec2::New(0.02F, 0.02F);
    particle.rotvel = rng::RandomFloat(-1.2F, 1.2F);
    particle.alpha_vel = -0.05F;
    particle.acc = Vec2::New(0.0F, 0.08F);
    particle.alpha_acc = -0.002F;
    state.particles.Add(std::move(particle));
}

void SpawnWebSpray(State& state, const Vec2& origin, const Vec2& direction) {
    const Vec2 normalized_direction = NormalizeOrZeroDeterministic(direction);
    for (int i = 0; i < 7; ++i) {
        const Vec2 spray_velocity =
            (normalized_direction * rng::RandomFloat(0.35F, 1.5F)) +
            Vec2::New(rng::RandomFloat(-0.45F, 0.45F), rng::RandomFloat(-0.45F, 0.45F));
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

void SpawnWebTrail(State& state, const Vec2& origin, const Vec2& base_vel) {
    for (int i = 0; i < 2; ++i) {
        SpawnWebParticle(
            state,
            origin,
            Vec2::New(
                base_vel.x * rng::RandomFloat(-0.10F, 0.04F),
                base_vel.y * rng::RandomFloat(-0.10F, 0.04F)
            ) + Vec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.08F, 0.08F)),
            rng::RandomFloat(0.35F, 0.6F),
            rng::RandomFloat(3.5F, 5.0F),
            rng::RandomFloat(6.0F, 10.0F)
        );
    }
}

void SpawnCobwebBurst(State& state, const Vec2& origin) {
    for (int i = 0; i < 6; ++i) {
        SpawnWebParticle(
            state,
            origin,
            Vec2::New(rng::RandomFloat(-0.9F, 0.9F), rng::RandomFloat(-1.0F, 0.15F)),
            rng::RandomFloat(0.65F, 0.9F),
            rng::RandomFloat(4.0F, 6.0F),
            rng::RandomFloat(8.0F, 14.0F)
        );
    }
}

Ent* SpawnCobwebEnt(State& state, const Vec2& center, bool temporary) {
    return world_ops::SpawnEnt(state, EntType::Cobweb, [&](Ent& cobweb) {
        cobweb.SetCenter(center);
        cobweb.counter_a = temporary ? kTemporaryCobwebLifetimeFrames : 0.0F;
        cobweb.counter_d = kCobwebWearIntervalFrames;
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

    SpawnCobwebBurst(state, cobweb.GetCenter());
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

    const Vec2 impact_center = web_ball.GetCenter();
    if (spawn_cobweb) {
        const IVec2 tile_pos = SnapWorldPointToTile(impact_center, state.stage);
        if (CanSpawnCobwebAtTile(tile_pos, state)) {
            (void)SpawnCobwebEnt(state, TileCenterToWorld(tile_pos), true);
        }
    }
    SpawnCobwebBurst(state, impact_center);
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

    const Vec2 impact_center = web_ball.GetCenter();
    if (CanSpawnCobwebAtTile(tile_pos, state)) {
        (void)SpawnCobwebEnt(state, TileCenterToWorld(tile_pos), true);
    }
    SpawnCobwebBurst(state, impact_center);
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

    const Vec2 muzzle_pos = weapon.GetCenter() + (aim.direction * 8.0F);
    const Vec2 spawn_pos = muzzle_pos + (aim.direction * 4.0F);

    if (holder != nullptr && IsWorldPointInsideSolidTile(muzzle_pos, state)) {
        const IVec2 holder_tile = SnapWorldPointToTile(holder->GetCenter(), state.stage);
        if (CanSpawnCobwebAtTile(holder_tile, state)) {
            (void)SpawnCobwebEnt(state, TileCenterToWorld(holder_tile), true);
        }
        (void)PlayWorldSoundEmitter(state, holder->GetCenter(), audio_asset_ids::PistolShoot);
        SpawnWebSpray(state, holder->GetCenter(), aim.direction);
        if (holder != nullptr) {
            holder->vel.x -= aim.direction.x * 0.12F;
            holder->vel.y -= aim.direction.y * 0.12F;
        }
        return;
    }

    (void)world_ops::SpawnEnt(state, EntType::WebBall, [&](Ent& spawned_web_ball) {
        spawned_web_ball.SetCenter(spawn_pos);
        spawned_web_ball.facing = aim.facing;
        spawned_web_ball.vel = (aim.direction * kWebBallSpeedX) +
                               (holder != nullptr ? holder->vel * 0.35F : Vec2::New(0.0F, 0.0F));
        spawned_web_ball.acc = Vec2::New(0.0F, 0.0F);
        spawned_web_ball.thrown_by =
            holder != nullptr ? std::optional<VID>(holder->vid) : weapon.use_state.user_vid;
        spawned_web_ball.thrown_immunity_timer = common::kThrownByImmunityDuration;
        spawned_web_ball.counter_a = kWebBallLifetimeFrames;
        spawned_web_ball.counter_b = 0.0F;
        spawned_web_ball.counter_c = kWebBallEntArmDelayFrames;
    });

    (void)PlayWorldSoundEmitter(state, muzzle_pos, audio_asset_ids::PistolShoot);
    SpawnWebSpray(state, muzzle_pos, aim.direction);
    if (holder != nullptr) {
        holder->vel.x -= aim.direction.x * 0.12F;
        holder->vel.y -= aim.direction.y * 0.12F;
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

    other.vel.x *= kCobwebHorizontalDamping;
    other.vel.y *= kCobwebVerticalDamping;
    other.acc.x *= kCobwebAccelerationDamping;
    other.acc.y *= kCobwebAccelerationDamping;
    if (std::abs(other.vel.x) < kCobwebOccupantSpeedThreshold) {
        other.vel.x = 0.0F;
    }
    if (std::abs(other.vel.y) < kCobwebOccupantSpeedThreshold) {
        other.vel.y = 0.0F;
    }
    other.fall_timer = 0;

    if (other.proj_contact_timer > 0) {
        other.proj_contact_timer = 0;
        const EntSpec& spec = GetEntSpec(other.type_);
        other.proj_contact_damage_type = spec.proj_contact_damage_type;
        other.proj_contact_damage_amount = spec.proj_contact_damage_amount;
    }

    const controls::ControlIntent intent = controls::GetControlIntentForEnt(other, state);
    if (intent.jump_pressed && other.vel.y > kCobwebJumpEscapeVelocity) {
        other.vel.y = kCobwebJumpEscapeVelocity;
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
    if (web_ball.counter_c > 0.0F) {
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
            other_mut->vel = Vec2::New(0.0F, 0.0F);
            other_mut->acc = Vec2::New(0.0F, 0.0F);
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

    const AABB cobweb_aabb = common::GetContactAabbForEnt(cobweb, *graphics);
    const AABB other_aabb = GetNearestWorldAabb(
        state.stage,
        cobweb.GetCenter(),
        common::GetContactAabbForEnt(other, *graphics)
    );
    if (!AabbsIntersect(cobweb_aabb, other_aabb)) {
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
    unsigned int amount,
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
    if (!weapon.use_state.pressed || weapon.counter_a > 0.0F) {
        return;
    }

    if (weapon.counter_b <= 0.0F) {
        weapon.counter_b = kWebGunBurstShots;
    }

    FireWebGun(ent_idx, state, graphics, audio);
    weapon.counter_b -= 1.0F;
    weapon.counter_a = weapon.counter_b <= 0.0F ? kWebGunReloadCooldownFrames : kWebGunFireCooldownFrames;

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
    if (weapon.counter_a > 0.0F) {
        weapon.counter_a -= 1.0F;
        if (weapon.counter_a < 0.0F) {
            weapon.counter_a = 0.0F;
        }
    }

    const Ent* holder = nullptr;
    if (weapon.held_by_vid.has_value()) {
        holder = state.ents.GetEnt(*weapon.held_by_vid);
    }
    if (holder == nullptr) {
        weapon.rotation = 0.0F;
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
    if (web_ball.counter_a > 0.0F) {
        web_ball.counter_a -= 1.0F;
    }
    if (web_ball.counter_a <= 0.0F) {
        TriggerWebBallBurst(ent_idx, state, true);
        return;
    }
    if (web_ball.counter_c > 0.0F) {
        web_ball.counter_c -= 1.0F;
        if (web_ball.counter_c < 0.0F) {
            web_ball.counter_c = 0.0F;
        }
    }

    web_ball.counter_b -= 1.0F;
    if (web_ball.counter_b <= 0.0F) {
        SpawnWebTrail(state, web_ball.GetCenter(), web_ball.vel);
        web_ball.counter_b = kWebBallTrailIntervalFrames;
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

    if (cobweb.counter_a > 0.0F) {
        cobweb.counter_a -= 1.0F;
        if (cobweb.counter_a <= 0.0F) {
            DestroyCobweb(ent_idx, state);
            return;
        }
    }

    const float health_ratio = static_cast<float>(cobweb.health) / static_cast<float>(std::max<std::uint32_t>(1, kCobwebDurability));
    const float lifetime_ratio = cobweb.counter_a > 0.0F
        ? std::clamp(cobweb.counter_a / kTemporaryCobwebLifetimeFrames, 0.0F, 1.0F)
        : 1.0F;
    cobweb.alpha = std::clamp(std::min(health_ratio, lifetime_ratio), 0.0F, 1.0F);

    const AABB cobweb_aabb = common::GetContactAabbForEnt(cobweb, graphics);
    const std::vector<VID> overlapped_vids = QueryEntsInAabb(state, cobweb_aabb, cobweb.vid);
    bool occupied = false;
    for (const VID& other_vid : overlapped_vids) {
        Ent* const other = state.ents.GetEntMut(other_vid);
        if (other == nullptr || !other->active) {
            continue;
        }

        const AABB other_aabb = GetNearestWorldAabb(
            state.stage,
            cobweb.GetCenter(),
            common::GetContactAabbForEnt(*other, graphics)
        );
        if (!AabbsIntersect(cobweb_aabb, other_aabb)) {
            continue;
        }

        if (!CanBeAffectedByCobweb(*other)) {
            continue;
        }

        const controls::ControlIntent intent = controls::GetControlIntentForEnt(*other, state);
        const bool moving_in_web = LengthSquared(other->vel) > kCobwebOccupantSpeedThresholdSq ||
                                   LengthSquared(other->acc) > 0.0F ||
                                   intent.jump_pressed;
        occupied = true;
        if (moving_in_web && cobweb.health > 0) {
            cobweb.counter_d -= 1.0F;
            if (cobweb.counter_d <= 0.0F) {
                cobweb.counter_d = kCobwebWearIntervalFrames;
                cobweb.health = std::max<std::uint32_t>(0, cobweb.health - 1);
                if (cobweb.health == 0) {
                    DestroyCobweb(ent_idx, state);
                    return;
                }
            }
        }
    }

    if (!occupied) {
        cobweb.counter_d = kCobwebWearIntervalFrames;
    }
}

extern const EntSpec kWebCannonSpec{
    .type_ = EntType::WebCannon,
    .size = Vec2::New(16.0F, 16.0F),
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
    .counter_b = kWebGunBurstShots,
    .damage_vuln = DamageVuln::Vulnerable,
    .on_use = OnUseAsWebCannon,
    .step_logic = StepEntLogicAsWebCannon,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::WebCannon),
};

extern const EntSpec kWebBallSpec{
    .type_ = EntType::WebBall,
    .size = Vec2::New(7.0F, 6.0F),
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
    .size = Vec2::New(16.0F, 16.0F),
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

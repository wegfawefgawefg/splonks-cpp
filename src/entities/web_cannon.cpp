#include "entities/web_cannon.hpp"

#include "audio.hpp"
#include "controls.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>

namespace splonks::entities::web_cannon {

namespace {

constexpr float kWebGunFireCooldownFrames = 10.0F;
constexpr float kWebGunReloadCooldownFrames = 42.0F;
constexpr float kWebGunBurstShots = 3.0F;
constexpr float kWebBallSpeedX = 6.5F;
constexpr float kWebBallSpeedY = -0.15F;
constexpr float kWebBallLifetimeFrames = 110.0F;
constexpr float kWebBallEntityArmDelayFrames = 2.0F;
constexpr float kWebBallTrailIntervalFrames = 3.0F;
constexpr float kTemporaryCobwebLifetimeFrames = 540.0F;
constexpr float kCobwebWearIntervalFrames = 6.0F;
constexpr std::uint32_t kCobwebDurability = 8;
constexpr float kCobwebHorizontalDamping = 0.18F;
constexpr float kCobwebVerticalDamping = 0.25F;
constexpr float kCobwebAccelerationDamping = 0.0F;
constexpr float kCobwebJumpEscapeVelocity = -1.7F;
constexpr float kCobwebOccupantSpeedThreshold = 0.05F;

struct WebGunAim {
    Vec2 direction = Vec2::New(1.0F, 0.0F);
    LeftOrRight facing = LeftOrRight::Right;
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

WebGunAim GetWebGunAim(const Entity& weapon, const Entity* holder, const State& state) {
    int aim_x = 0;
    int aim_y = 0;
    LeftOrRight facing = holder != nullptr ? holder->facing : weapon.facing;
    if (holder != nullptr) {
        const controls::ControlIntent intent = controls::GetControlIntentForEntity(*holder, state);
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
        facing = LeftOrRight::Left;
    } else if (aim_x > 0) {
        facing = LeftOrRight::Right;
    }

    Vec2 direction = Vec2::New(
        static_cast<float>(aim_x),
        static_cast<float>(aim_y)
    );
    if (direction == Vec2::New(0.0F, 0.0F)) {
        direction = facing == LeftOrRight::Left ? Vec2::New(-1.0F, 0.0F) : Vec2::New(1.0F, 0.0F);
    } else {
        direction = NormalizeOrZero(direction);
    }

    const float world_angle = std::atan2(direction.y, direction.x) * (180.0F / 3.14159265F);
    const float base_angle = facing == LeftOrRight::Left ? 180.0F : 0.0F;
    return WebGunAim{
        .direction = direction,
        .facing = facing,
        .rotation = NormalizeDegrees(world_angle - base_angle),
    };
}

IVec2 SnapWorldPointToTile(const Vec2& point, const Stage& stage) {
    const int tile_x = static_cast<int>(std::floor(point.x / static_cast<float>(kTileSize)));
    const int tile_y = static_cast<int>(std::floor(point.y / static_cast<float>(kTileSize)));
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
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active || entity.type_ != EntityType::Cobweb) {
            continue;
        }
        if (SnapWorldPointToTile(entity.GetCenter(), state.stage) == wrapped_tile_pos) {
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

std::optional<IVec2> GetCobwebGrowthTile(const Entity& web_ball, const Entity& hit_cobweb, const State& state) {
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
        incoming_dir = web_ball.facing == LeftOrRight::Left ? Vec2::New(1.0F, 0.0F) : Vec2::New(-1.0F, 0.0F);
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
    for (const VID& other_vid : QueryEntitiesInAabb(state, target_aabb, self_vid)) {
        const Entity* const other = state.entity_manager.GetEntity(other_vid);
        if (other == nullptr || !other->active || other->type_ != EntityType::Cobweb) {
            continue;
        }

        const AABB cobweb_aabb = GetNearestWorldAabb(
            state.stage,
            (target_aabb.tl + target_aabb.br) * 0.5F,
            common::GetContactAabbForEntity(*other, graphics)
        );
        if (AabbsIntersect(target_aabb, cobweb_aabb)) {
            return true;
        }
    }
    return false;
}

bool CanBeAffectedByCobweb(const Entity& entity) {
    return entity.active && entity.affected_by_cobweb && !entity.held_by_vid.has_value();
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
    particle.frame_data_animator = FrameDataAnimator::New(frame_data_ids::WebBall);
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
    const Vec2 normalized_direction = NormalizeOrZero(direction);
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

Entity* SpawnWebBallEntity(State& state, const Vec2& pos) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const web_ball = state.entity_manager.GetEntityMut(*vid);
    if (web_ball == nullptr) {
        return nullptr;
    }

    SetEntityAs(*web_ball, EntityType::WebBall);
    web_ball->SetCenter(pos);
    return web_ball;
}

Entity* SpawnCobwebEntity(State& state, const Vec2& center, bool temporary) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const cobweb = state.entity_manager.GetEntityMut(*vid);
    if (cobweb == nullptr) {
        return nullptr;
    }

    SetEntityAs(*cobweb, EntityType::Cobweb);
    cobweb->SetCenter(center);
    cobweb->counter_a = temporary ? kTemporaryCobwebLifetimeFrames : 0.0F;
    cobweb->counter_d = kCobwebWearIntervalFrames;
    cobweb->health = kCobwebDurability;
    return cobweb;
}

void DestroyCobweb(std::size_t entity_idx, State& state) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& cobweb = state.entity_manager.entities[entity_idx];
    if (!cobweb.active) {
        return;
    }

    SpawnCobwebBurst(state, cobweb.GetCenter());
    state.entity_manager.SetInactive(entity_idx);
}

void TriggerWebBallBurst(std::size_t entity_idx, State& state, bool spawn_cobweb) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& web_ball = state.entity_manager.entities[entity_idx];
    if (!web_ball.active) {
        return;
    }

    const Vec2 impact_center = web_ball.GetCenter();
    if (spawn_cobweb) {
        const IVec2 tile_pos = SnapWorldPointToTile(impact_center, state.stage);
        if (CanSpawnCobwebAtTile(tile_pos, state)) {
            (void)SpawnCobwebEntity(state, TileCenterToWorld(tile_pos), true);
        }
    }
    SpawnCobwebBurst(state, impact_center);
    state.entity_manager.SetInactive(entity_idx);
}

void TriggerWebBallBurstAtTile(std::size_t entity_idx, State& state, const IVec2& tile_pos) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& web_ball = state.entity_manager.entities[entity_idx];
    if (!web_ball.active) {
        return;
    }

    const Vec2 impact_center = web_ball.GetCenter();
    if (CanSpawnCobwebAtTile(tile_pos, state)) {
        (void)SpawnCobwebEntity(state, TileCenterToWorld(tile_pos), true);
    }
    SpawnCobwebBurst(state, impact_center);
    state.entity_manager.SetInactive(entity_idx);
}

void FireWebGun(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Entity& weapon = state.entity_manager.entities[entity_idx];
    Entity* holder = nullptr;
    if (weapon.held_by_vid.has_value()) {
        holder = state.entity_manager.GetEntityMut(*weapon.held_by_vid);
    }

    const WebGunAim aim = GetWebGunAim(weapon, holder, state);
    weapon.facing = aim.facing;
    weapon.rotation = aim.rotation;

    const Vec2 muzzle_pos = weapon.GetCenter() + (aim.direction * 8.0F);
    const Vec2 spawn_pos = muzzle_pos + (aim.direction * 4.0F);

    if (holder != nullptr && IsWorldPointInsideSolidTile(muzzle_pos, state)) {
        const IVec2 holder_tile = SnapWorldPointToTile(holder->GetCenter(), state.stage);
        if (CanSpawnCobwebAtTile(holder_tile, state)) {
            (void)SpawnCobwebEntity(state, TileCenterToWorld(holder_tile), true);
        }
        (void)PlayWorldSoundEmitter(state, holder->GetCenter(), audio_asset_ids::PistolShoot);
        SpawnWebSpray(state, holder->GetCenter(), aim.direction);
        if (holder != nullptr) {
            holder->vel.x -= aim.direction.x * 0.12F;
            holder->vel.y -= aim.direction.y * 0.12F;
        }
        return;
    }

    Entity* const web_ball = SpawnWebBallEntity(state, spawn_pos);
    if (web_ball != nullptr) {
        web_ball->facing = aim.facing;
        web_ball->vel = (aim.direction * kWebBallSpeedX) +
                        (holder != nullptr ? holder->vel * 0.35F : Vec2::New(0.0F, 0.0F));
        web_ball->acc = Vec2::New(0.0F, 0.0F);
        web_ball->thrown_by = holder != nullptr ? std::optional<VID>(holder->vid) : weapon.use_state.user_vid;
        web_ball->thrown_immunity_timer = common::kThrownByImmunityDuration;
        web_ball->counter_a = kWebBallLifetimeFrames;
        web_ball->counter_b = 0.0F;
        web_ball->counter_c = kWebBallEntityArmDelayFrames;
    }

    (void)PlayWorldSoundEmitter(state, muzzle_pos, audio_asset_ids::PistolShoot);
    SpawnWebSpray(state, muzzle_pos, aim.direction);
    if (holder != nullptr) {
        holder->vel.x -= aim.direction.x * 0.12F;
        holder->vel.y -= aim.direction.y * 0.12F;
    }
}

bool ApplyCobwebToEntity(std::size_t cobweb_idx, Entity& other, State& state) {
    if (cobweb_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& cobweb = state.entity_manager.entities[cobweb_idx];
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
    other.fall_distance = 0.0F;

    if (other.projectile_contact_timer > 0) {
        other.projectile_contact_timer = 0;
        const EntityArchetype& archetype = GetEntityArchetype(other.type_);
        other.projectile_contact_damage_type = archetype.projectile_contact_damage_type;
        other.projectile_contact_damage_amount = archetype.projectile_contact_damage_amount;
    }

    const controls::ControlIntent intent = controls::GetControlIntentForEntity(other, state);
    if (intent.jump_pressed && other.vel.y > kCobwebJumpEscapeVelocity) {
        other.vel.y = kCobwebJumpEscapeVelocity;
    }

    return true;
}

common::ContactResolution OnEntityContactAsWebBall(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    if (context.phase != common::ContactPhase::SweptEntered ||
        !context.mover_vid.has_value() ||
        *context.mover_vid != state.entity_manager.entities[entity_idx].vid) {
        return common::ContactResolution{};
    }
    if (entity_idx >= state.entity_manager.entities.size() || other_entity_idx >= state.entity_manager.entities.size()) {
        return common::ContactResolution{};
    }

    const Entity& web_ball = state.entity_manager.entities[entity_idx];
    const Entity& other = state.entity_manager.entities[other_entity_idx];
    if (!web_ball.active || !other.active) {
        return common::ContactResolution{};
    }
    if (web_ball.counter_c > 0.0F) {
        return common::ContactResolution{};
    }
    if (web_ball.thrown_by.has_value() && other.vid == *web_ball.thrown_by) {
        return common::ContactResolution{};
    }
    if (web_ball.thrown_by.has_value() && other.held_by_vid.has_value() &&
        *other.held_by_vid == *web_ball.thrown_by) {
        return common::ContactResolution{};
    }

    if (audio != nullptr) {
        switch (other.type_) {
        case EntityType::Pot:
        case EntityType::Box:
        case EntityType::Skull:
            (void)common::TryDamageEntity(other_entity_idx, state, *audio, DamageType::Attack, 1);
            break;
        default:
            break;
        }
    }

    if (other.type_ != EntityType::Cobweb) {
        if (Entity* const other_mut = state.entity_manager.GetEntityMut(other.vid)) {
            other_mut->vel = Vec2::New(0.0F, 0.0F);
            other_mut->acc = Vec2::New(0.0F, 0.0F);
            other_mut->fall_distance = 0.0F;
        }
        TriggerWebBallBurst(entity_idx, state, true);
        return common::ContactResolution{.stop_sweep = true};
    }

    if (const std::optional<IVec2> growth_tile = GetCobwebGrowthTile(web_ball, other, state); growth_tile.has_value()) {
        TriggerWebBallBurstAtTile(entity_idx, state, *growth_tile);
    } else {
        TriggerWebBallBurst(entity_idx, state, false);
    }
    return common::ContactResolution{.stop_sweep = true};
}

common::ContactResolution OnTileContactAsWebBall(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    if (context.phase != common::ContactPhase::AttemptedBlocked) {
        return common::ContactResolution{};
    }

    TriggerWebBallBurst(entity_idx, state, true);
    return common::ContactResolution{.stop_sweep = true};
}

common::ContactResolution OnEntityContactAsCobweb(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)audio;
    if (graphics == nullptr || context.phase != common::ContactPhase::SweptEntered) {
        return common::ContactResolution{};
    }
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return common::ContactResolution{};
    }

    Entity& cobweb = state.entity_manager.entities[entity_idx];
    Entity& other = state.entity_manager.entities[other_entity_idx];
    if (!cobweb.active || cobweb.type_ != EntityType::Cobweb || !CanBeAffectedByCobweb(other)) {
        return common::ContactResolution{};
    }
    if (!context.mover_vid.has_value() || *context.mover_vid != other.vid) {
        return common::ContactResolution{};
    }

    const AABB cobweb_aabb = common::GetContactAabbForEntity(cobweb, *graphics);
    const AABB other_aabb = GetNearestWorldAabb(
        state.stage,
        cobweb.GetCenter(),
        common::GetContactAabbForEntity(other, *graphics)
    );
    if (!AabbsIntersect(cobweb_aabb, other_aabb)) {
        return common::ContactResolution{};
    }

    const bool applied = ApplyCobwebToEntity(entity_idx, other, state);
    if (!applied) {
        return common::ContactResolution{};
    }

    const bool is_controlled = state.controlled_entity_vid.has_value() &&
                               other.vid == *state.controlled_entity_vid;
    const bool is_player = other.type_ == EntityType::Player;
    if (is_controlled || is_player) {
        return common::ContactResolution{};
    }

    other.grounded = true;
    return common::ContactResolution{.stop_sweep = true};
}

EntityDamageEffectResult OnDamageAsCobweb(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    (void)audio;
    (void)amount;
    (void)damage_applied;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return EntityDamageEffectResult::None;
    }

    Entity& cobweb = state.entity_manager.entities[entity_idx];
    if (!cobweb.active || cobweb.type_ != EntityType::Cobweb) {
        return EntityDamageEffectResult::None;
    }

    switch (damage_type) {
    case DamageType::JumpOn:
        return EntityDamageEffectResult::None;
    case DamageType::Attack:
        if (amount >= cobweb.health) {
            cobweb.health = 0;
            DestroyCobweb(entity_idx, state);
        } else {
            cobweb.health -= amount;
        }
        return EntityDamageEffectResult::Consumed;
    case DamageType::IgnitingAttack:
    case DamageType::Burn:
    case DamageType::Explosion:
    case DamageType::Crush:
        cobweb.health = 0;
        DestroyCobweb(entity_idx, state);
        return EntityDamageEffectResult::Consumed;
    default:
        return EntityDamageEffectResult::None;
    }
}

} // namespace

void OnUseAsWebCannon(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    Entity& weapon = state.entity_manager.entities[entity_idx];
    if (!weapon.use_state.pressed || weapon.counter_a > 0.0F) {
        return;
    }

    if (weapon.counter_b <= 0.0F) {
        weapon.counter_b = kWebGunBurstShots;
    }

    FireWebGun(entity_idx, state, graphics, audio);
    weapon.counter_b -= 1.0F;
    weapon.counter_a = weapon.counter_b <= 0.0F ? kWebGunReloadCooldownFrames : kWebGunFireCooldownFrames;

    if (weapon.use_state.source == AttachmentMode::None) {
        StopUsingEntity(weapon);
    }
}

void StepEntityLogicAsWebCannon(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Entity& weapon = state.entity_manager.entities[entity_idx];
    if (weapon.counter_a > 0.0F) {
        weapon.counter_a -= 1.0F;
        if (weapon.counter_a < 0.0F) {
            weapon.counter_a = 0.0F;
        }
    }

    const Entity* holder = nullptr;
    if (weapon.held_by_vid.has_value()) {
        holder = state.entity_manager.GetEntity(*weapon.held_by_vid);
    }
    if (holder == nullptr) {
        weapon.rotation = 0.0F;
        return;
    }

    const WebGunAim aim = GetWebGunAim(weapon, holder, state);
    weapon.facing = aim.facing;
    weapon.rotation = aim.rotation;
}

void StepEntityLogicAsWebBall(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Entity& web_ball = state.entity_manager.entities[entity_idx];
    if (web_ball.counter_a > 0.0F) {
        web_ball.counter_a -= 1.0F;
    }
    if (web_ball.counter_a <= 0.0F) {
        TriggerWebBallBurst(entity_idx, state, true);
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

void StepEntityPhysicsAsWebBall(
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

    Entity& web_ball = state.entity_manager.entities[entity_idx];
    if (!web_ball.active) {
        return;
    }
    if (web_ball.collided) {
        TriggerWebBallBurst(entity_idx, state, true);
    }
}

void StepEntityLogicAsCobweb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& cobweb = state.entity_manager.entities[entity_idx];
    if (!cobweb.active || cobweb.type_ != EntityType::Cobweb) {
        return;
    }

    if (cobweb.counter_a > 0.0F) {
        cobweb.counter_a -= 1.0F;
        if (cobweb.counter_a <= 0.0F) {
            DestroyCobweb(entity_idx, state);
            return;
        }
    }

    const float health_ratio = static_cast<float>(cobweb.health) / static_cast<float>(std::max<std::uint32_t>(1, kCobwebDurability));
    const float lifetime_ratio = cobweb.counter_a > 0.0F
        ? std::clamp(cobweb.counter_a / kTemporaryCobwebLifetimeFrames, 0.0F, 1.0F)
        : 1.0F;
    cobweb.alpha = std::clamp(std::min(health_ratio, lifetime_ratio), 0.0F, 1.0F);

    const AABB cobweb_aabb = common::GetContactAabbForEntity(cobweb, graphics);
    const std::vector<VID> overlapped_vids = QueryEntitiesInAabb(state, cobweb_aabb, cobweb.vid);
    bool occupied = false;
    for (const VID& other_vid : overlapped_vids) {
        Entity* const other = state.entity_manager.GetEntityMut(other_vid);
        if (other == nullptr || !other->active) {
            continue;
        }

        const AABB other_aabb = GetNearestWorldAabb(
            state.stage,
            cobweb.GetCenter(),
            common::GetContactAabbForEntity(*other, graphics)
        );
        if (!AabbsIntersect(cobweb_aabb, other_aabb)) {
            continue;
        }

        if (!CanBeAffectedByCobweb(*other)) {
            continue;
        }

        const controls::ControlIntent intent = controls::GetControlIntentForEntity(*other, state);
        const bool moving_in_web = Length(other->vel) > kCobwebOccupantSpeedThreshold ||
                                   Length(other->acc) > 0.0F ||
                                   intent.jump_pressed;
        occupied = true;
        if (moving_in_web && cobweb.health > 0) {
            cobweb.counter_d -= 1.0F;
            if (cobweb.counter_d <= 0.0F) {
                cobweb.counter_d = kCobwebWearIntervalFrames;
                cobweb.health = std::max<std::uint32_t>(0, cobweb.health - 1);
                if (cobweb.health == 0) {
                    DestroyCobweb(entity_idx, state);
                    return;
                }
            }
        }
    }

    if (!occupied) {
        cobweb.counter_d = kCobwebWearIntervalFrames;
    }
}

extern const EntityArchetype kWebCannonArchetype{
    .type_ = EntityType::WebCannon,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .counter_b = kWebGunBurstShots,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_use = OnUseAsWebCannon,
    .step_logic = StepEntityLogicAsWebCannon,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::WebCannon),
};

extern const EntityArchetype kWebBallArchetype{
    .type_ = EntityType::WebBall,
    .size = Vec2::New(7.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = false,
    .can_receive_projectile_contact = false,
    .can_be_picked_up = false,
    .affected_by_cobweb = false,
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
    .can_apply_projectile_contact = false,
    .step_logic = StepEntityLogicAsWebBall,
    .step_physics = StepEntityPhysicsAsWebBall,
    .on_entity_contact = OnEntityContactAsWebBall,
    .on_tile_contact = OnTileContactAsWebBall,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::WebBall),
};

extern const EntityArchetype kCobwebArchetype{
    .type_ = EntityType::Cobweb,
    .size = Vec2::New(16.0F, 16.0F),
    .health = kCobwebDurability,
    .has_physics = false,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_projectile_contact = false,
    .can_be_picked_up = false,
    .affected_by_cobweb = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Background,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .can_apply_projectile_contact = false,
    .on_damage = OnDamageAsCobweb,
    .step_logic = StepEntityLogicAsCobweb,
    .on_entity_contact = OnEntityContactAsCobweb,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Cobweb),
};

} // namespace splonks::entities::web_cannon

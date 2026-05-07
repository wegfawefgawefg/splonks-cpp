#include "entities/common/common.hpp"
#include "particles/sprite_particle.hpp"

#include "presentation_commands.hpp"
#include "stage_break.hpp"
#include "stage_lighting.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <memory>
#include <array>

namespace splonks::entities::common {

namespace {

constexpr int kExplosionCrossRadiusTiles = 2;
constexpr int kExplosionDiagonalRadiusTiles = 1;
constexpr float kExplosionShakeForegroundAmount = 4.0F;
constexpr float kExplosionShakeBackgroundAmount = 3.0F;
constexpr float kExplosionShakeEntityAmount = 3.5F;
constexpr float kExplosionShakeRadiusTiles = 3.0F;

bool IsInSpelunkyExplosionFootprint(const IVec2& tile_delta) {
    const int abs_x = std::abs(tile_delta.x);
    const int abs_y = std::abs(tile_delta.y);
    if (abs_x <= kExplosionDiagonalRadiusTiles && abs_y <= kExplosionDiagonalRadiusTiles) {
        return true;
    }
    return (abs_x == 0 && abs_y <= kExplosionCrossRadiusTiles) ||
           (abs_y == 0 && abs_x <= kExplosionCrossRadiusTiles);
}

bool IsInSpelunkyExplosionFootprint(const Vec2& world_delta) {
    const float tile_dx = world_delta.x / static_cast<float>(kTileSize);
    const float tile_dy = world_delta.y / static_cast<float>(kTileSize);
    const float abs_x = std::abs(tile_dx);
    const float abs_y = std::abs(tile_dy);
    if (abs_x <= 1.5F && abs_y <= 1.5F) {
        return true;
    }
    return (abs_x <= 0.5F && abs_y <= 2.5F) || (abs_y <= 0.5F && abs_x <= 2.5F);
}

std::vector<IVec2> BuildExplosionFootprintTiles(const Stage& stage, const Vec2& center) {
    const IVec2 center_tile = stage.GetTileCoordAtWc(ToIVec2(center));
    std::vector<IVec2> result;
    result.reserve(13);
    for (int dy = -kExplosionCrossRadiusTiles; dy <= kExplosionCrossRadiusTiles; ++dy) {
        for (int dx = -kExplosionCrossRadiusTiles; dx <= kExplosionCrossRadiusTiles; ++dx) {
            const IVec2 delta = IVec2::New(dx, dy);
            if (!IsInSpelunkyExplosionFootprint(delta)) {
                continue;
            }
            const IVec2 tile_pos = center_tile + delta;
            if (!stage.WrapsX() && !stage.WrapsY() && !stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                continue;
            }
            result.push_back(tile_pos);
        }
    }
    return result;
}

} // namespace

void DoExplosion(
    std::size_t entity_idx,
    Vec2 center,
    float size,
    float push_magnitude,
    State& state,
    Audio& audio
) {
    const float effect_size = size * 0.5F * static_cast<float>(kTileSize);
    {
        SpriteParticle effect{};
        effect.frame_data_animator = FrameDataAnimator::New(frame_data_ids::GrenadeBoom);
        effect.frame_data_animator.loop = false;
        effect.finish_on_animation_end = true;
        effect.draw_layer = DrawLayer::Foreground;
        effect.lighting_mode = ParticleLightingMode::Emissive;
        effect.counter = 8;
        effect.pos = center;
        effect.size = Vec2::New(effect_size, effect_size);
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = 1.0F;
        effect.vel = Vec2::New(0.0F, 0.0F);
        effect.svel = Vec2::New(2.0F, 2.0F);
        effect.rotvel = 0.0F;
        effect.alpha_vel = 0.0F;
        effect.acc = Vec2::New(0.0F, 0.0F);
        effect.sacc = Vec2::New(-0.2F, -0.2F);
        effect.rotacc = 0.0F;
        effect.alpha_acc = 0.0F;
        state.particles.Add(std::move(effect));
    }
    for (int i = 0; i < 16; ++i) {
        const float vel = rng::RandomFloat(-0.3F, 0.0F);
        const float svel = rng::RandomFloat(-vel * 0.1F, -vel * 1.0F);
        const float sacc = rng::RandomFloat(-vel * 0.01F, -vel * 0.02F);

        SpriteParticle effect{};
        effect.frame_data_animator = FrameDataAnimator::New(frame_data_ids::BigSmoke);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(64, 128));
        effect.pos = center;
        effect.size = Vec2::New(0.0F, 0.0F);
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = 1.0F;
        effect.vel = Vec2::New(0.0F, rng::RandomFloat(-0.3F, 0.0F));
        effect.svel = Vec2::New(svel, svel);
        effect.rotvel = rng::RandomFloat(-0.2F, -0.01F);
        effect.alpha_vel = vel * 0.001F;
        effect.acc = Vec2::New(0.0F, 0.0F);
        effect.sacc = Vec2::New(sacc, sacc);
        effect.rotacc = 0.0F;
        effect.alpha_acc = 0.0F;
        state.particles.Add(std::move(effect));
    }
    (void)PlayWorldSoundEmitter(state, center, audio_asset_ids::BombExplosion);
    world_ops::QueuePresentationCommand(state, PresentationCommand{
        .kind = PresentationCommandKind::PlaySoundAt,
        .audio_asset_id = audio_asset_ids::BombExplosion,
        .source_pos = center,
    });
    AddShake(
        state,
        center,
        kExplosionShakeForegroundAmount,
        kExplosionShakeBackgroundAmount,
        kExplosionShakeEntityAmount,
        kExplosionShakeRadiusTiles
    );
    world_ops::QueuePresentationCommand(state, PresentationCommand{
        .kind = PresentationCommandKind::ShakeArea,
        .source_pos = center,
        .param_a = kExplosionShakeForegroundAmount,
        .param_b = kExplosionShakeBackgroundAmount,
        .param_c = kExplosionShakeEntityAmount,
        .param_d = kExplosionShakeRadiusTiles,
    });
    AddTransientLight(state, center, 2.4F, Color3::New(1.0F, 0.48F, 0.12F), 9, 14);

    const std::vector<IVec2> explosion_tiles = BuildExplosionFootprintTiles(state.stage, center);
    BreakStageTilesAtCoords(explosion_tiles, state, audio);

    const float explosion_size = size * static_cast<float>(kTileSize);
    const AABB area = {
        .tl = center - (Vec2::New(1.0F, 1.0F) * explosion_size),
        .br = center + (Vec2::New(1.0F, 1.0F) * explosion_size),
    };

    const VID this_vid = state.entity_manager.GetVid(entity_idx);
    const std::vector<VID> results = QueryEntitiesInAabb(state, area, this_vid);
    for (const VID& vid : results) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(vid)) {
            const Vec2 delta = GetNearestWorldDelta(state.stage, center, entity->GetCenter());
            const bool can_receive_push =
                entity->active &&
                entity->has_physics &&
                !entity->impassable &&
                IsInSpelunkyExplosionFootprint(delta);
            if (!can_receive_push) {
                (void)TryDamageEntity(
                    vid.id,
                    state,
                    audio,
                    DamageType::Explosion,
                    10,
                    DamageOptions{
                        .source_vid = this_vid,
                        .allow_remote_player_target = true,
                    }
                );
                continue;
            }
            Vec2 knockback_dir = NormalizeOrZero(delta);
            if (knockback_dir == Vec2::New(0.0F, 0.0F)) {
                knockback_dir = Vec2::New(0.0F, -1.0F);
            }
            (void)TryHitEntity(
                vid.id,
                state,
                audio,
                DamageType::Explosion,
                10,
                HitOptions{
                    .source_vid = this_vid,
                    .knockback = KnockbackSpec{
                        .velocity = knockback_dir * push_magnitude,
                        .clear_velocity = true,
                        .clear_acceleration = true,
                        .thrown_by = this_vid,
                        .projectile_contact_damage_type = DamageType::Attack,
                        .projectile_contact_damage_amount = 1,
                        .projectile_contact_duration = kProjectileContactDuration,
                    },
                    .allow_remote_player_target = true,
                }
            );
        }
    }
}

} // namespace splonks::entities::common

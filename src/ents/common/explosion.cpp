#include "ents/common/common.hpp"
#include "particles/sprite_particle.hpp"

#include "stage_break.hpp"
#include "stage_lighting.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <memory>
#include <array>

namespace splonks::ents::common {

namespace {

constexpr int kExplosionCrossRadiusTiles = 2;
constexpr int kExplosionDiagonalRadiusTiles = 1;
constexpr float kExplosionShakeForegroundAmount = 4.0F;
constexpr float kExplosionShakeBackgroundAmount = 3.0F;
constexpr float kExplosionShakeEntAmount = 3.5F;
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

bool IsInSpelunkyExplosionFootprint(FxVec2 world_delta) {
    const FxScalar abs_x = world_delta.x.abs();
    const FxScalar abs_y = world_delta.y.abs();
    const FxScalar diagonal_limit = ToFxScalar(1.5F * static_cast<float>(kTileSize));
    if (abs_x <= diagonal_limit && abs_y <= diagonal_limit) {
        return true;
    }

    const FxScalar centerline_limit = ToFxScalar(0.5F * static_cast<float>(kTileSize));
    const FxScalar cross_limit = ToFxScalar(2.5F * static_cast<float>(kTileSize));
    return (abs_x <= centerline_limit && abs_y <= cross_limit) ||
           (abs_y <= centerline_limit && abs_x <= cross_limit);
}

std::vector<IVec2> BuildExplosionFootprintTiles(const Stage& stage, FxVec2 center) {
    const IVec2 center_tile = stage.GetTileCoordAtWc(
        IVec2::New(center.x.trunc_int(), center.y.trunc_int())
    );
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
    std::size_t ent_idx,
    FxVec2 center,
    float size,
    float push_magnitude,
    State& state,
    Audio& audio
) {
    const FVec2 render_center = ToFVec2(center);
    const float effect_size = size * 0.5F * static_cast<float>(kTileSize);
    {
        SpriteParticle effect{};
        effect.aframe_animator = AFrameAnimator::New(aframe_ids::GrenadeBoom);
        effect.aframe_animator.loop = false;
        effect.finish_on_anim_end = true;
        effect.draw_layer = DrawLayer::Foreground;
        effect.lighting_mode = ParticleLightingMode::Emissive;
        effect.counter = 8;
        effect.pos = render_center;
        effect.size = FVec2::New(effect_size, effect_size);
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = 1.0F;
        effect.vel = FVec2::New(0.0F, 0.0F);
        effect.svel = FVec2::New(2.0F, 2.0F);
        effect.rotvel = 0.0F;
        effect.alpha_vel = 0.0F;
        effect.acc = FVec2::New(0.0F, 0.0F);
        effect.sacc = FVec2::New(-0.2F, -0.2F);
        effect.rotacc = 0.0F;
        effect.alpha_acc = 0.0F;
        state.particles.Add(std::move(effect));
    }
    for (int i = 0; i < 16; ++i) {
        const float vel = rng::RandomFloat(-0.3F, 0.0F);
        const float svel = rng::RandomFloat(-vel * 0.1F, -vel * 1.0F);
        const float sacc = rng::RandomFloat(-vel * 0.01F, -vel * 0.02F);

        SpriteParticle effect{};
        effect.aframe_animator = AFrameAnimator::New(aframe_ids::BigSmoke);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(64, 128));
        effect.pos = render_center;
        effect.size = FVec2::New(0.0F, 0.0F);
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = 1.0F;
        effect.vel = FVec2::New(0.0F, rng::RandomFloat(-0.3F, 0.0F));
        effect.svel = FVec2::New(svel, svel);
        effect.rotvel = rng::RandomFloat(-0.2F, -0.01F);
        effect.alpha_vel = vel * 0.001F;
        effect.acc = FVec2::New(0.0F, 0.0F);
        effect.sacc = FVec2::New(sacc, sacc);
        effect.rotacc = 0.0F;
        effect.alpha_acc = 0.0F;
        state.particles.Add(std::move(effect));
    }
    (void)PlayWorldSoundEmitter(state, render_center, audio_asset_ids::BombExplosion);
    AddShake(
        state,
        render_center,
        kExplosionShakeForegroundAmount,
        kExplosionShakeBackgroundAmount,
        kExplosionShakeEntAmount,
        kExplosionShakeRadiusTiles
    );
    const Color3 explosion_light_color = Color3::New(1.0F, 0.48F, 0.12F);
    AddTransientLight(state, render_center, 2.4F, explosion_light_color, 9, 14);

    const std::vector<IVec2> explosion_tiles = BuildExplosionFootprintTiles(state.stage, center);
    BreakStageTilesAtCoords(explosion_tiles, state, audio);

    const FxScalar explosion_size = ToFxScalar(size * static_cast<float>(kTileSize));
    const FxAABB area = FxAABB::from_corners(
        center - FxVec2{explosion_size, explosion_size},
        center + FxVec2{explosion_size, explosion_size}
    );

    const VID this_vid = state.ents.GetVid(ent_idx);
    const std::vector<VID> results = QueryEntsInAabb(state, area, this_vid);
    const FxScalar sim_push_magnitude = ToFxScalar(push_magnitude);
    for (const VID& vid : results) {
        if (Ent* const ent = state.ents.GetEntMut(vid)) {
            const FxVec2 delta = GetNearestWorldDelta(state.stage, center, ent->GetCenter());
            const bool can_receive_push =
                ent->active &&
                ent->has_physics &&
                !ent->impassable &&
                IsInSpelunkyExplosionFootprint(delta);
            if (!can_receive_push) {
                (void)TryDamageEnt(
                    vid.id,
                    state,
                    audio,
                    DamageType::Explosion,
                    10,
                    DamageOptions{
                        .source_vid = this_vid,
                    }
                );
                continue;
            }
            FxVec2 knockback_dir = FxNormalizeOrZero(delta);
            if (knockback_dir == FxVec2::zero()) {
                knockback_dir = FxVec2{
                    FxScalar::zero(),
                    FxScalar::from_int(-1),
                };
            }
            (void)TryHitEnt(
                vid.id,
                state,
                audio,
                DamageType::Explosion,
                10,
                HitOptions{
                    .source_vid = this_vid,
                    .knockback = KnockbackSpec{
                        .velocity = knockback_dir * sim_push_magnitude,
                        .clear_velocity = true,
                        .clear_acceleration = true,
                        .thrown_by = this_vid,
                        .proj_contact_damage_type = DamageType::Attack,
                        .proj_contact_damage_amount = 1,
                        .proj_contact_duration = kProjContactDuration,
                    },
                }
            );
        }
    }
}

} // namespace splonks::ents::common

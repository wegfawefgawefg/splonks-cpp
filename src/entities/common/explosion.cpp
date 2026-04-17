#include "entities/common/common.hpp"

#include "particles/ultra_dynamic_particle.hpp"
#include "stage_break.hpp"
#include "world_query.hpp"

#include <memory>

namespace splonks::entities::common {

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
        auto effect = std::make_unique<UltraDynamicParticle>();
        effect->frame_data_animator = FrameDataAnimator::New(frame_data_ids::GrenadeBoom);
        effect->frame_data_animator.loop = false;
        effect->finish_on_animation_end = true;
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = 8;
        effect->pos = center;
        effect->size = Vec2::New(effect_size, effect_size);
        effect->rot = rng::RandomFloat(0.0F, 360.0F);
        effect->alpha = 1.0F;
        effect->vel = Vec2::New(0.0F, 0.0F);
        effect->svel = Vec2::New(2.0F, 2.0F);
        effect->rotvel = 0.0F;
        effect->alpha_vel = 0.0F;
        effect->acc = Vec2::New(0.0F, 0.0F);
        effect->sacc = Vec2::New(-0.2F, -0.2F);
        effect->rotacc = 0.0F;
        effect->alpha_acc = 0.0F;
        state.particles.Add(std::move(effect));
    }
    for (int i = 0; i < 16; ++i) {
        const float vel = rng::RandomFloat(-0.3F, 0.0F);
        const float svel = rng::RandomFloat(-vel * 0.1F, -vel * 1.0F);
        const float sacc = rng::RandomFloat(-vel * 0.01F, -vel * 0.02F);

        auto effect = std::make_unique<UltraDynamicParticle>();
        effect->frame_data_animator = FrameDataAnimator::New(frame_data_ids::BigSmoke);
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(64, 128));
        effect->pos = center;
        effect->size = Vec2::New(0.0F, 0.0F);
        effect->rot = rng::RandomFloat(0.0F, 360.0F);
        effect->alpha = 1.0F;
        effect->vel = Vec2::New(0.0F, rng::RandomFloat(-0.3F, 0.0F));
        effect->svel = Vec2::New(svel, svel);
        effect->rotvel = rng::RandomFloat(-0.2F, -0.01F);
        effect->alpha_vel = vel * 0.001F;
        effect->acc = Vec2::New(0.0F, 0.0F);
        effect->sacc = Vec2::New(sacc, sacc);
        effect->rotacc = 0.0F;
        effect->alpha_acc = 0.0F;
        state.particles.Add(std::move(effect));
    }
    audio.PlaySoundEffect(SoundEffect::BombExplosion);
    const float explosion_size = size * static_cast<float>(kTileSize);
    const AABB area = {
        .tl = center - (Vec2::New(1.0F, 1.0F) * explosion_size),
        .br = center + (Vec2::New(1.0F, 1.0F) * explosion_size),
    };
    BreakStageTilesInRectWc(area, state, audio);

    const VID this_vid = state.entity_manager.GetVid(entity_idx);
    const std::vector<VID> results = QueryEntitiesInAabb(state, area, this_vid);
    for (const VID& vid : results) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(vid)) {
            const DamageResult damage_result = TryDamageEntity(vid.id, state, audio, DamageType::Explosion, 10);
            if ((damage_result == DamageResult::Hurt || damage_result == DamageResult::Died) && entity->active) {
                const Vec2 delta = GetNearestWorldDelta(state.stage, center, entity->GetCenter());
                Vec2 knockback_dir = NormalizeOrZero(delta);
                if (knockback_dir == Vec2::New(0.0F, 0.0F)) {
                    knockback_dir = Vec2::New(0.0F, -1.0F);
                }
                ApplyKnockback(
                    *entity,
                    KnockbackSpec{
                        .velocity = knockback_dir * push_magnitude,
                        .clear_velocity = true,
                        .clear_acceleration = true,
                        .projectile_contact_damage_type = DamageType::Attack,
                        .projectile_contact_damage_amount = 1,
                        .projectile_contact_duration = kProjectileContactDuration,
                    }
                );
            }
        }
    }
}

} // namespace splonks::entities::common

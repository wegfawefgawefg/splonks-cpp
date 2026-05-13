#include "presentation_commands.hpp"

#include "audio_emitters.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "particles/ribbon_particle.hpp"
#include "particles/sprite_particle.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace splonks {

namespace {

Vec2 GetDirectionAxis(const IVec2& direction) {
    const Vec2 axis = Vec2::New(static_cast<float>(direction.x), static_cast<float>(direction.y));
    const float length = Length(axis);
    if (length <= 0.0F) {
        return Vec2::New(1.0F, 0.0F);
    }
    return axis / length;
}

Vec2 GetDirectionOrtho(const Vec2& axis) {
    return Vec2::New(-axis.y, axis.x);
}

void SpawnEntityPhaseParticleAt(
    const Entity& entity,
    const Graphics& graphics,
    const Vec2& visual_center,
    const Vec2& start_offset,
    const Vec2& velocity,
    float tint_r,
    float tint_g,
    float tint_b,
    State& state
) {
    const FrameData* const frame_data = entities::common::GetCurrentFrameDataForEntity(entity, graphics);
    if (frame_data == nullptr) {
        return;
    }

    SpriteParticle particle{};
    particle.counter = 32;
    particle.draw_layer = entity.draw_layer;
    particle.lighting_mode = ParticleLightingMode::Emissive;
    particle.pos = visual_center + start_offset;
    particle.size = Vec2::New(
        static_cast<float>(frame_data->sample_rect.w),
        static_cast<float>(frame_data->sample_rect.h)
    ) * entity.frame_data_animator.scale;
    particle.rot = entity.rotation;
    particle.alpha = 0.85F;
    particle.tint_r = tint_r;
    particle.tint_g = tint_g;
    particle.tint_b = tint_b;
    particle.horizontal_flip = entity.facing == LeftOrRight::Right;
    particle.vel = velocity;
    particle.alpha_vel = -0.0275F;
    particle.frame_data_animator = entity.frame_data_animator;
    particle.frame_data_animator.animate = false;
    state.particles.Add(std::move(particle));
}

void SpawnTeleportSplitEffectAt(
    const Entity& entity,
    const Graphics& graphics,
    const Vec2& visual_center,
    const IVec2& direction,
    State& state
) {
    const Vec2 axis = GetDirectionAxis(direction);
    const Vec2 ortho = GetDirectionOrtho(axis);
    SpawnEntityPhaseParticleAt(entity, graphics, visual_center, Vec2::New(0.0F, 0.0F), (axis * -0.3F) - (ortho * 0.0625F), 1.0F, 0.20F, 0.20F, state);
    SpawnEntityPhaseParticleAt(entity, graphics, visual_center, Vec2::New(0.0F, 0.0F), ortho * 0.0375F, 0.25F, 1.0F, 0.25F, state);
    SpawnEntityPhaseParticleAt(entity, graphics, visual_center, Vec2::New(0.0F, 0.0F), (axis * 0.3F) - (ortho * 0.0625F), 0.30F, 0.30F, 1.0F, state);
}

void SpawnTeleportMergeEffectAt(
    const Entity& entity,
    const Graphics& graphics,
    const Vec2& visual_center,
    const IVec2& direction,
    State& state
) {
    const Vec2 axis = GetDirectionAxis(direction);
    const Vec2 ortho = GetDirectionOrtho(axis);
    SpawnEntityPhaseParticleAt(entity, graphics, visual_center, axis * -3.0F, axis * 0.3F, 1.0F, 0.20F, 0.20F, state);
    SpawnEntityPhaseParticleAt(entity, graphics, visual_center, ortho * 2.0F, ortho * -0.0875F, 0.25F, 1.0F, 0.25F, state);
    SpawnEntityPhaseParticleAt(entity, graphics, visual_center, axis * 3.0F, axis * -0.3F, 0.30F, 0.30F, 1.0F, state);
}

void SpawnJetpackSmokeAt(State& state, const Vec2& pos) {
    for (int i = 0; i < 16; ++i) {
        const float vel = rng::RandomFloat(0.1F, 0.5F);
        const float svel = rng::RandomFloat(vel * 0.1F, vel * 1.0F);
        const float sacc = rng::RandomFloat(vel * 0.01F, vel * 0.02F);
        SpriteParticle effect{};
        effect.frame_data_animator = FrameDataAnimator::New(frame_data_ids::BigSmoke);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(0, 32));
        effect.pos = pos;
        effect.size = Vec2::New(1.0F, 1.0F) * 2.0F;
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = 1.0F;
        effect.vel = Vec2::New(0.0F, rng::RandomFloat(0.0F, 0.3F));
        effect.svel = Vec2::New(svel, svel);
        effect.rotvel = rng::RandomFloat(-0.2F, -0.01F);
        effect.alpha_vel = vel * 0.001F;
        effect.sacc = Vec2::New(sacc, sacc);
        state.particles.Add(std::move(effect));
    }
}

void SpawnExplosionBurstAt(State& state, const Vec2& center, float size) {
    const float effect_size = std::max(size, 0.0F) * 0.5F * static_cast<float>(kTileSize);
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
        effect.svel = Vec2::New(2.0F, 2.0F);
        effect.sacc = Vec2::New(-0.2F, -0.2F);
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
        effect.sacc = Vec2::New(sacc, sacc);
        state.particles.Add(std::move(effect));
    }
}

void SpawnPistolMuzzleSmokeAt(State& state, const Vec2& pos, int direction) {
    const int normalized_direction = direction < 0 ? -1 : 1;
    for (int i = 0; i < 5; ++i) {
        SpriteParticle effect{};
        effect.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(8, 14));
        effect.pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        effect.size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(3.0F, 5.0F));
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = rng::RandomFloat(0.75F, 0.95F);
        effect.vel = Vec2::New(
            rng::RandomFloat(0.05F, 0.25F) * static_cast<float>(normalized_direction),
            rng::RandomFloat(-0.18F, -0.04F)
        );
        effect.svel = Vec2::New(rng::RandomFloat(0.08F, 0.20F), rng::RandomFloat(0.08F, 0.20F));
        effect.rotvel = rng::RandomFloat(-1.5F, 1.5F);
        effect.alpha_vel = -0.05F;
        effect.acc = Vec2::New(0.0F, -0.01F);
        effect.sacc = Vec2::New(0.01F, 0.01F);
        effect.alpha_acc = -0.003F;
        state.particles.Add(std::move(effect));
    }
}

void SpawnPistolImpactAt(State& state, const Vec2& pos, int direction) {
    const int normalized_direction = direction < 0 ? -1 : 1;
    for (int i = 0; i < 3; ++i) {
        SpriteParticle spark{};
        spark.frame_data_animator = FrameDataAnimator::New(frame_data_ids::Spark);
        spark.draw_layer = DrawLayer::Foreground;
        spark.lighting_mode = ParticleLightingMode::Emissive;
        spark.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(5, 9));
        spark.pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        spark.size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(4.0F, 6.0F));
        spark.rot = rng::RandomFloat(0.0F, 360.0F);
        spark.alpha = 1.0F;
        spark.vel = Vec2::New(
            rng::RandomFloat(-0.45F, -0.10F) * static_cast<float>(normalized_direction),
            rng::RandomFloat(-0.18F, 0.18F)
        );
        spark.svel = Vec2::New(-0.12F, -0.12F);
        spark.rotvel = rng::RandomFloat(-6.0F, 6.0F);
        spark.alpha_vel = -0.14F;
        state.particles.Add(std::move(spark));
    }

    for (int i = 0; i < 2; ++i) {
        SpriteParticle smoke{};
        smoke.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(10, 16));
        smoke.pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        smoke.size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(3.0F, 5.0F));
        smoke.rot = rng::RandomFloat(0.0F, 360.0F);
        smoke.alpha = rng::RandomFloat(0.75F, 0.95F);
        smoke.vel = Vec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.18F, -0.06F));
        smoke.svel = Vec2::New(rng::RandomFloat(0.06F, 0.14F), rng::RandomFloat(0.06F, 0.14F));
        smoke.rotvel = rng::RandomFloat(-1.5F, 1.5F);
        smoke.alpha_vel = -0.05F;
        smoke.acc = Vec2::New(0.0F, -0.01F);
        smoke.sacc = Vec2::New(0.01F, 0.01F);
        smoke.alpha_acc = -0.003F;
        state.particles.Add(std::move(smoke));
    }
}

void SpawnTreasurePickupSparklesAt(State& state, const Vec2& center, Color3 color, std::uint32_t count) {
    const int particle_count = std::clamp(static_cast<int>(count), 1, 12);
    for (int i = 0; i < particle_count; ++i) {
        SpriteParticle particle{};
        particle.frame_data_animator = FrameDataAnimator::New(
            rng::RandomIntInclusive(0, 1) == 0 ? frame_data_ids::Sparkle : frame_data_ids::Glint
        );
        particle.frame_data_animator.loop = false;
        particle.draw_layer = DrawLayer::Foreground;
        particle.lighting_mode = ParticleLightingMode::Emissive;
        particle.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(12, 22));
        particle.pos = center + Vec2::New(
            rng::RandomFloat(-3.0F, 3.0F),
            rng::RandomFloat(-3.0F, 2.0F)
        );
        const float size = rng::RandomFloat(3.0F, 6.0F);
        particle.size = Vec2::New(size, size);
        particle.rot = rng::RandomFloat(0.0F, 360.0F);
        particle.alpha = rng::RandomFloat(0.80F, 1.0F);
        particle.tint_r = color.r;
        particle.tint_g = color.g;
        particle.tint_b = color.b;
        particle.vel = Vec2::New(
            rng::RandomFloat(-0.55F, 0.55F),
            rng::RandomFloat(-0.85F, -0.25F)
        );
        particle.svel = Vec2::New(-0.02F, -0.02F);
        particle.rotvel = rng::RandomFloat(-12.0F, 12.0F);
        particle.alpha_vel = rng::RandomFloat(-0.035F, -0.020F);
        particle.acc = Vec2::New(0.0F, 0.035F);
        particle.sacc = Vec2::New(-0.002F, -0.002F);
        particle.alpha_acc = -0.002F;
        state.particles.Add(std::move(particle));
    }
}

void SpawnBaseballBatTrailAt(State& state, const Vec2& from, const Vec2& to) {
    const Vec2 wrapped_to = GetNearestWorldPoint(state.stage, from, to);
    if (Length(wrapped_to - from) < 2.0F) {
        return;
    }

    RibbonParticle ribbon{};
    ribbon.counter = 6;
    ribbon.archetype_id = ribbon_particle_archetype_ids::BaseballBatTrail;
    ribbon.alpha = 0.36F;
    ribbon.point_count = 2;
    ribbon.points[0] = from;
    ribbon.points[1] = wrapped_to;
    state.particles.Add(std::move(ribbon));
}

void PlayScriptedEffect(State& state, Graphics& graphics, const PresentationCommand& command) {
    if (command.effect_id == ScriptedPresentationEffectId::JetpackSmoke) {
        SpawnJetpackSmokeAt(state, command.source_pos + Vec2::New(3.0F, 3.0F));
        SpawnJetpackSmokeAt(state, command.source_pos + Vec2::New(-3.0F, 3.0F));
        return;
    }
    if (command.effect_id == ScriptedPresentationEffectId::ExplosionBurst) {
        SpawnExplosionBurstAt(state, command.source_pos, command.effect_scale);
        return;
    }
    if (command.effect_id == ScriptedPresentationEffectId::PistolMuzzleSmoke) {
        SpawnPistolMuzzleSmokeAt(state, command.source_pos, command.direction.x);
        return;
    }
    if (command.effect_id == ScriptedPresentationEffectId::PistolImpact) {
        SpawnPistolImpactAt(state, command.source_pos, command.direction.x);
        return;
    }
    if (command.effect_id == ScriptedPresentationEffectId::TreasurePickupSparkles) {
        SpawnTreasurePickupSparklesAt(state, command.source_pos, command.light_color, command.effect_count);
        return;
    }
    if (command.effect_id == ScriptedPresentationEffectId::BaseballBatTrail) {
        SpawnBaseballBatTrailAt(state, command.source_pos, command.target_pos);
        return;
    }
    if (!command.source_vid.has_value()) {
        return;
    }
    const Entity* const entity = state.entity_manager.GetEntity(*command.source_vid);
    if (entity == nullptr || !entity->active) {
        return;
    }

    switch (command.effect_id) {
    case ScriptedPresentationEffectId::TeleportSplit:
        SpawnTeleportSplitEffectAt(*entity, graphics, command.source_pos, command.direction, state);
        break;
    case ScriptedPresentationEffectId::TeleportMerge:
        SpawnTeleportMergeEffectAt(*entity, graphics, command.source_pos, command.direction, state);
        break;
    case ScriptedPresentationEffectId::JetpackSmoke:
    case ScriptedPresentationEffectId::ExplosionBurst:
    case ScriptedPresentationEffectId::PistolMuzzleSmoke:
    case ScriptedPresentationEffectId::PistolImpact:
    case ScriptedPresentationEffectId::TreasurePickupSparkles:
    case ScriptedPresentationEffectId::BaseballBatTrail:
        break;
    case ScriptedPresentationEffectId::None:
        break;
    }
}

} // namespace

void PlayPresentationCommand(State& state, Graphics& graphics, const PresentationCommand& command) {
    switch (command.kind) {
    case PresentationCommandKind::PlaySoundAt:
        if (command.audio_asset_id != kInvalidAudioAssetId) {
            (void)PlayWorldSoundEmitter(state, command.source_pos, command.audio_asset_id);
        }
        break;
    case PresentationCommandKind::ShakeEntity:
        if (command.source_vid.has_value()) {
            if (Entity* const entity = state.entity_manager.GetEntityMut(*command.source_vid)) {
                if (entity->active) {
                    AddEntityShake(*entity, command.entity_shake_amount);
                }
            }
        }
        break;
    case PresentationCommandKind::ShakeArea:
        AddShake(
            state,
            command.source_pos,
            command.foreground_shake_amount,
            command.background_shake_amount,
            command.area_entity_shake_amount,
            command.shake_radius_tiles,
            command.source_vid
        );
        break;
    case PresentationCommandKind::SpawnScriptedEffect:
        PlayScriptedEffect(state, graphics, command);
        break;
    case PresentationCommandKind::AddTransientLight:
        AddTransientLight(
            state,
            command.source_pos,
            command.light_strength,
            command.light_color,
            command.light_radius,
            command.light_lifetime_frames
        );
        break;
    case PresentationCommandKind::None:
        break;
    }
}

} // namespace splonks

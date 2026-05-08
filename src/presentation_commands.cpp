#include "presentation_commands.hpp"

#include "audio_emitters.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"

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

void PlayScriptedEffect(State& state, Graphics& graphics, const PresentationCommand& command) {
    if (command.effect_id == ScriptedPresentationEffectId::JetpackSmoke) {
        SpawnJetpackSmokeAt(state, command.source_pos + Vec2::New(3.0F, 3.0F));
        SpawnJetpackSmokeAt(state, command.source_pos + Vec2::New(-3.0F, 3.0F));
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
                    AddEntityShake(*entity, command.param_a);
                }
            }
        }
        break;
    case PresentationCommandKind::ShakeArea:
        AddShake(
            state,
            command.source_pos,
            command.param_a,
            command.param_b,
            command.param_c,
            command.param_d,
            command.source_vid
        );
        break;
    case PresentationCommandKind::SpawnScriptedEffect:
        PlayScriptedEffect(state, graphics, command);
        break;
    case PresentationCommandKind::None:
        break;
    }
}

} // namespace splonks

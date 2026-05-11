#include "effects/treasure_pickup.hpp"

#include "entity.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "particles/lighting_mode.hpp"
#include "particles/sprite_particle.hpp"
#include "presentation_commands.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <utility>

namespace splonks::effects {

void SpawnTreasurePickupSparkles(const Entity& pickup, State& state, Color3 color, int count) {
    const Vec2 center = pickup.GetCenter();
    const int particle_count = std::clamp(count, 1, 12);
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
    world_ops::QueuePresentationCommand(state, PresentationCommand{
        .kind = PresentationCommandKind::SpawnScriptedEffect,
        .effect_id = ScriptedPresentationEffectId::TreasurePickupSparkles,
        .source_pos = center,
        .effect_count = static_cast<std::uint32_t>(particle_count),
        .light_color = color,
    });

    AddTransientLight(state, center, 0.85F, color, 3, 6);
    world_ops::QueuePresentationCommand(state, PresentationCommand{
        .kind = PresentationCommandKind::AddTransientLight,
        .source_pos = center,
        .light_strength = 0.85F,
        .light_color = color,
        .light_radius = 3,
        .light_lifetime_frames = 6,
    });
}

} // namespace splonks::effects

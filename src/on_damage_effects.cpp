#include "on_damage_effects.hpp"
#include "particles/sprite_particle.hpp"

#include "utils.hpp"

#include <memory>

namespace splonks {

namespace {

} // namespace

void SpawnDamageEffectAnimBurst(AFrameId anim_id, const FVec2& center, State& state) {
    constexpr float kVelRange = 8.0F;

    for (int i = 0; i < 16; ++i) {
        SpriteParticle effect{};
        effect.aframe_animator = AFrameAnimator::New(anim_id);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = 16;
        effect.pos = center;
        effect.size = FVec2::New(4.0F, 4.0F);
        effect.rot = 0.0F;
        effect.alpha = 1.0F;
        effect.vel = FVec2::New(rng::RandomFloat(-kVelRange, kVelRange), rng::RandomFloat(-kVelRange, kVelRange));
        effect.svel = FVec2::New(-1.0F, -1.0F);
        effect.rotvel = 0.0F;
        effect.alpha_vel = -0.1F;
        state.particles.Add(std::move(effect));
    }
}

void SpawnBreakawayContainerShards(const FVec2& center, State& state) {
    SpawnDamageEffectAnimBurst(aframe_ids::LittleBrownShard, center, state);
}

} // namespace splonks

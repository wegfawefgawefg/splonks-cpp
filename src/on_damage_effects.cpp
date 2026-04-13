#include "on_damage_effects.hpp"

#include "particles/dynamic_particle.hpp"
#include "utils.hpp"

#include <memory>

namespace splonks {

namespace {

} // namespace

void SpawnDamageEffectAnimationBurst(FrameDataId animation_id, const Vec2& center, State& state) {
    constexpr float kVelRange = 8.0F;

    for (int i = 0; i < 16; ++i) {
        auto effect = std::make_unique<DynamicParticle>();
        effect->frame_data_animator = FrameDataAnimator::New(animation_id);
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = 16;
        effect->pos = center;
        effect->size = Vec2::New(4.0F, 4.0F);
        effect->rot = 0.0F;
        effect->alpha = 1.0F;
        effect->vel = Vec2::New(rng::RandomFloat(-kVelRange, kVelRange), rng::RandomFloat(-kVelRange, kVelRange));
        effect->svel = Vec2::New(-1.0F, -1.0F);
        effect->rotvel = 0.0F;
        effect->alpha_vel = -0.1F;
        state.particles.Add(std::move(effect));
    }
}

void SpawnBreakawayContainerShards(const Vec2& center, State& state) {
    SpawnDamageEffectAnimationBurst(frame_data_ids::LittleBrownShard, center, state);
}

} // namespace splonks

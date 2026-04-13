#include "on_damage_effects.hpp"

#include "special_effects/dynamic_effect.hpp"

#include <memory>
#include <random>

namespace splonks {

namespace {

float RandomFloat(float minimum, float maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(generator);
}

} // namespace

void SpawnDamageEffectAnimationBurst(FrameDataId animation_id, const Vec2& center, State& state) {
    constexpr float kVelRange = 8.0F;

    for (int i = 0; i < 16; ++i) {
        auto effect = std::make_unique<DynamicEffect>();
        effect->frame_data_animator = FrameDataAnimator::New(animation_id);
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = 16;
        effect->pos = center;
        effect->size = Vec2::New(4.0F, 4.0F);
        effect->rot = 0.0F;
        effect->alpha = 1.0F;
        effect->vel = Vec2::New(RandomFloat(-kVelRange, kVelRange), RandomFloat(-kVelRange, kVelRange));
        effect->svel = Vec2::New(-1.0F, -1.0F);
        effect->rotvel = 0.0F;
        effect->alpha_vel = -0.1F;
        state.special_effects.push_back(std::move(effect));
    }
}

void SpawnBreakawayContainerShards(const Vec2& center, State& state) {
    SpawnDamageEffectAnimationBurst(frame_data_ids::LittleBrownShard, center, state);
}

} // namespace splonks

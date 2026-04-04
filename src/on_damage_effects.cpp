#include "on_damage_effects.hpp"

#include "special_effects/dynamic_effect.hpp"
#include "special_effects/ultra_dynamic_effect.hpp"

#include <memory>
#include <random>

namespace splonks {

namespace {

int RandomIntExclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_int_distribution<int> distribution(minimum, maximum - 1);
    return distribution(generator);
}

float RandomFloat(float minimum, float maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(generator);
}

} // namespace

void OnDamageEffectAsBreakawayContainer(std::size_t entity_idx, State& state) {
    const Entity& breakaway_container = state.entity_manager.entities[entity_idx];
    const Vec2 center = breakaway_container.GetCenter();
    constexpr float kVelRange = 8.0F;

    for (int i = 0; i < 16; ++i) {
        auto effect = std::make_unique<DynamicEffect>();
        effect->type_ = SpecialEffectType::LittleBrownShard;
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

void OnDamageEffectAsBleedingEntity(std::size_t entity_idx, State& state) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const Vec2 center = entity.GetCenter();
    constexpr float kVelRange = 8.0F;

    for (int i = 0; i < 16; ++i) {
        auto effect = std::make_unique<DynamicEffect>();
        effect->type_ = SpecialEffectType::BloodBall;
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

void OnDeathEffectAsExplosive(std::size_t entity_idx, State& state) {
    Entity& bomb = state.entity_manager.entities[entity_idx];
    const Vec2 center = bomb.GetCenter();
    constexpr int kSize = 2;
    const float effect_size = static_cast<float>(kSize) * 0.5F * static_cast<float>(kTileSize);

    {
        auto effect = std::make_unique<UltraDynamicEffect>();
        effect->type_ = SpecialEffectType::GrenadeBoom;
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = 8;
        effect->pos = center;
        effect->size = Vec2::New(effect_size, effect_size);
        effect->rot = RandomFloat(0.0F, 360.0F);
        effect->alpha = 1.0F;
        effect->vel = Vec2::New(0.0F, 0.0F);
        effect->svel = Vec2::New(2.0F, 2.0F);
        effect->rotvel = 0.0F;
        effect->alpha_vel = 0.0F;
        effect->acc = Vec2::New(0.0F, 0.0F);
        effect->sacc = Vec2::New(-0.2F, -0.2F);
        effect->rotacc = 0.0F;
        effect->alpha_acc = -0.0F;
        state.special_effects.push_back(std::move(effect));
    }

    for (int i = 0; i < 16; ++i) {
        const float vel = RandomFloat(-0.3F, 0.0F);
        const float svel = RandomFloat(-vel * 0.1F, -vel * 1.0F);
        const float sacc = RandomFloat(-vel * 0.01F, -vel * 0.02F);

        auto effect = std::make_unique<UltraDynamicEffect>();
        effect->type_ = SpecialEffectType::BasicSmoke;
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = static_cast<std::uint32_t>(RandomIntExclusive(64, 128));
        effect->pos = center;
        effect->size = Vec2::New(0.0F, 0.0F);
        effect->rot = RandomFloat(0.0F, 360.0F);
        effect->alpha = 1.0F;
        effect->vel = Vec2::New(0.0F, RandomFloat(-0.3F, 0.0F));
        effect->svel = Vec2::New(svel, svel);
        effect->rotvel = RandomFloat(-0.2F, -0.01F);
        effect->alpha_vel = vel * 0.001F;
        effect->acc = Vec2::New(0.0F, 0.0F);
        effect->sacc = Vec2::New(sacc, sacc);
        effect->rotacc = 0.0F;
        effect->alpha_acc = 0.0F;
        state.special_effects.push_back(std::move(effect));
    }
}

} // namespace splonks

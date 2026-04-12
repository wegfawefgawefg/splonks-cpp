#pragma once

#include "entity/draw_layer.hpp"
#include "special_effects/special_effect.hpp"

namespace splonks {

struct UltraDynamicEffect final : public SpecialEffect {
    SpecialEffectType type_ = SpecialEffectType::GrenadeBoom;
    std::uint32_t counter = 0;
    DrawLayer draw_layer = DrawLayer::Middle;
    Vec2 pos;
    Vec2 size;
    float rot = 0.0F;
    float alpha = 1.0F;
    Vec2 vel;
    Vec2 svel;
    float rotvel = 0.0F;
    float alpha_vel = 0.0F;
    Vec2 acc;
    Vec2 sacc;
    float rotacc = 0.0F;
    float alpha_acc = 0.0F;

    void Step() override;
    bool IsFinished() const override;
    Vec2 GetPos() const override;
    Vec2 GetSize() const override;
    float GetRot() const override;
    std::uint32_t GetCounter() const override;
    SpecialEffectType GetType() const override;
    float GetAlpha() const override;
    const SampleRegion& GetSampleRegion() const override;
};

} // namespace splonks

#pragma once

#include "draw_layer.hpp"
#include "special_effects/special_effect.hpp"

namespace splonks {

struct UltraDynamicEffect final : public SpecialEffect {
    std::uint32_t counter = 0;
    bool finish_on_animation_end = false;
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
    FrameDataAnimator frame_data_animator{};

    void Step(const FrameDataDb& frame_data_db, float dt) override;
    bool IsFinished() const override;
    Vec2 GetPos() const override;
    Vec2 GetSize() const override;
    float GetRot() const override;
    float GetAlpha() const override;
    const FrameDataAnimator& GetFrameDataAnimator() const override;
};

} // namespace splonks

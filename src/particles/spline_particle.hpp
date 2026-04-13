#pragma once

#include "draw_layer.hpp"
#include "particles/particle.hpp"

namespace splonks {

struct SplineParticle final : public Particle {
    std::uint32_t counter = 0;
    bool finish_on_animation_end = false;
    DrawLayer draw_layer = DrawLayer::Middle;
    Vec2 point_1;
    Vec2 point_2;
    Vec2 point_3;
    float t = 0.0F;
    Vec2 pos;
    Vec2 size;
    float rot = 0.0F;
    float alpha = 1.0F;
    Vec2 tvel;
    Vec2 svel;
    float rotvel = 0.0F;
    float alpha_vel = 0.0F;
    Vec2 tacc;
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

Vec2 CalculateBezierPoint(float t, const Vec2& point_1, const Vec2& point_2, const Vec2& point_3);

} // namespace splonks

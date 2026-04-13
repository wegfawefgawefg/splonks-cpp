#include "particles/spline_particle.hpp"

namespace splonks {

void SplineParticle::Step(const FrameDataDb& frame_data_db, float dt) {
    if (counter > 0) {
        counter -= 1;
    }

    frame_data_animator.Step(frame_data_db, dt);
    tvel += tacc;
    svel += sacc;
    rotvel += rotacc;
    alpha_vel += alpha_acc;

    t += tvel.x;
    size += svel;
    rot += rotvel;
    alpha += alpha_vel;

    size = Max(size, Vec2::New(0.0F, 0.0F));
    alpha = Max(Min(alpha, 1.0F), 0.0F);
    t = Max(Min(t, 1.0F), 0.0F);
    pos = CalculateBezierPoint(t, point_1, point_2, point_3);
}

bool SplineParticle::IsFinished() const {
    return counter == 0 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

Vec2 SplineParticle::GetPos() const {
    return pos;
}

Vec2 SplineParticle::GetSize() const {
    return size;
}

float SplineParticle::GetRot() const {
    return rot;
}

float SplineParticle::GetAlpha() const {
    return alpha;
}

const FrameDataAnimator& SplineParticle::GetFrameDataAnimator() const {
    return frame_data_animator;
}

Vec2 CalculateBezierPoint(float t, const Vec2& point_1, const Vec2& point_2, const Vec2& point_3) {
    const float one_minus_t = 1.0F - t;
    return (point_1 * one_minus_t * one_minus_t) + (point_2 * 2.0F * one_minus_t * t) +
           (point_3 * t * t);
}

} // namespace splonks

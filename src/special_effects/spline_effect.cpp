#include "special_effects/spline_effect.hpp"

namespace splonks {

void SplineEffect::Step() {
    if (counter > 0) {
        counter -= 1;
    }

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

bool SplineEffect::IsFinished() const {
    return counter <= 0;
}

Vec2 SplineEffect::GetPos() const {
    return pos;
}

Vec2 SplineEffect::GetSize() const {
    return size;
}

float SplineEffect::GetRot() const {
    return rot;
}

std::uint32_t SplineEffect::GetCounter() const {
    return counter;
}

SpecialEffectType SplineEffect::GetType() const {
    return type_;
}

float SplineEffect::GetAlpha() const {
    return alpha;
}

const SampleRegion& SplineEffect::GetSampleRegion() const {
    return splonks::GetSampleRegion(GetType(), GetCounter());
}

Vec2 CalculateBezierPoint(float t, const Vec2& point_1, const Vec2& point_2, const Vec2& point_3) {
    const float one_minus_t = 1.0F - t;
    return (point_1 * one_minus_t * one_minus_t) + (point_2 * 2.0F * one_minus_t * t) +
           (point_3 * t * t);
}

} // namespace splonks

#include "special_effects/dynamic_effect.hpp"

namespace splonks {

void DynamicEffect::Step() {
    if (counter > 0) {
        counter -= 1;
    }

    pos += vel;
    size += svel;
    rot += rotvel;
    alpha += alpha_vel;

    size = Max(size, Vec2::New(0.0F, 0.0F));
    alpha = Max(Min(alpha, 1.0F), 0.0F);
}

bool DynamicEffect::IsFinished() const {
    return counter <= 0;
}

Vec2 DynamicEffect::GetPos() const {
    return pos;
}

Vec2 DynamicEffect::GetSize() const {
    return size;
}

float DynamicEffect::GetRot() const {
    return rot;
}

std::uint32_t DynamicEffect::GetCounter() const {
    return counter;
}

SpecialEffectType DynamicEffect::GetType() const {
    return type_;
}

float DynamicEffect::GetAlpha() const {
    return alpha;
}

const SampleRegion& DynamicEffect::GetSampleRegion() const {
    return splonks::GetSampleRegion(GetType(), GetCounter());
}

} // namespace splonks

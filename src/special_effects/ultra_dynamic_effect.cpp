#include "special_effects/ultra_dynamic_effect.hpp"

namespace splonks {

void UltraDynamicEffect::Step() {
    if (counter > 0) {
        counter -= 1;
    }

    vel += acc;
    svel += sacc;
    rotvel += rotacc;
    alpha_vel += alpha_acc;

    pos += vel;
    size += svel;
    rot += rotvel;
    alpha += alpha_vel;

    size = Max(size, Vec2::New(0.0F, 0.0F));
    alpha = Max(Min(alpha, 1.0F), 0.0F);
}

bool UltraDynamicEffect::IsFinished() const {
    return counter <= 0;
}

Vec2 UltraDynamicEffect::GetPos() const {
    return pos;
}

Vec2 UltraDynamicEffect::GetSize() const {
    return size;
}

float UltraDynamicEffect::GetRot() const {
    return rot;
}

std::uint32_t UltraDynamicEffect::GetCounter() const {
    return counter;
}

SpecialEffectType UltraDynamicEffect::GetType() const {
    return type_;
}

float UltraDynamicEffect::GetAlpha() const {
    return alpha;
}

const SampleRegion& UltraDynamicEffect::GetSampleRegion() const {
    return splonks::GetSampleRegion(GetType(), GetCounter());
}

} // namespace splonks

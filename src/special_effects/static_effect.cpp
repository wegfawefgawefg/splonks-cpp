#include "special_effects/static_effect.hpp"

namespace splonks {

void StaticEffect::Step() {
    if (counter > 0) {
        counter -= 1;
    }
}

bool StaticEffect::IsFinished() const {
    return counter <= 0;
}

Vec2 StaticEffect::GetPos() const {
    return pos;
}

Vec2 StaticEffect::GetSize() const {
    return size;
}

float StaticEffect::GetRot() const {
    return rot;
}

std::uint32_t StaticEffect::GetCounter() const {
    return counter;
}

SpecialEffectType StaticEffect::GetType() const {
    return type_;
}

float StaticEffect::GetAlpha() const {
    return alpha;
}

const SampleRegion& StaticEffect::GetSampleRegion() const {
    return splonks::GetSampleRegion(GetType(), GetCounter());
}

} // namespace splonks

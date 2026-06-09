#include "utils.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <random>

namespace splonks {

DetRng DetRng::New(std::uint32_t seed) {
    DetRng rng;
    rng.state = (static_cast<std::uint64_t>(seed) << 32U) ^ 0xD1B54A32D192ED03ULL;
    (void)rng.NextU32();
    return rng;
}

std::uint32_t DetRng::NextU32() {
    std::uint64_t value = (state += 0x9E3779B97F4A7C15ULL);
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    value = value ^ (value >> 31U);
    return static_cast<std::uint32_t>(value >> 32U);
}

int DetRng::RandomIntInclusive(int minimum, int maximum) {
    assert(minimum <= maximum);
    const std::uint32_t span = static_cast<std::uint32_t>(maximum - minimum + 1);
    return minimum + static_cast<int>(NextU32() % span);
}

int DetRng::RandomIntExclusive(int minimum, int maximum) {
    assert(minimum < maximum);
    return RandomIntInclusive(minimum, maximum - 1);
}

bool DetRng::RandomBool() {
    return RandomIntInclusive(0, 1) == 0;
}

float DetRng::RandomFloat(float minimum, float maximum) {
    const std::uint32_t mantissa = NextU32() >> 8U;
    const float unit = static_cast<float>(mantissa) * (1.0F / 16777216.0F);
    return minimum + (maximum - minimum) * std::clamp(unit, 0.0F, 1.0F);
}

FxScalar RandomFxScalar(DetRng& rng, FxScalar minimum, FxScalar maximum) {
    assert(minimum <= maximum);
    return FxScalar::from_raw(rng.RandomIntInclusive(
        minimum.raw_value(),
        maximum.raw_value()
    ));
}

FAABB FAABB::New(const FVec2& top_left, const FVec2& bottom_right) {
    FAABB result;
    result.tl = top_left;
    result.br = bottom_right;
    return result;
}

IAABB FAABB::AsIAABB() const {
    IAABB result;
    result.tl = IVec2::New(static_cast<int>(tl.x), static_cast<int>(tl.y));
    result.br = IVec2::New(static_cast<int>(br.x), static_cast<int>(br.y));
    return result;
}

IAABB IAABB::New(const IVec2& top_left, const IVec2& bottom_right) {
    IAABB result;
    result.tl = top_left;
    result.br = bottom_right;
    return result;
}

FAABB IAABB::AsFAABB() const {
    FAABB result;
    result.tl = FVec2::New(static_cast<float>(tl.x), static_cast<float>(tl.y));
    result.br = FVec2::New(static_cast<float>(br.x), static_cast<float>(br.y));
    return result;
}


namespace rng {

namespace {

std::uint32_t MakeProcessRandomSeed() {
    static std::random_device device;
    const std::uint32_t seed = device();
    return seed == 0 ? 1U : seed;
}

DetRng& GetRandomGenerator() {
    static DetRng generator = DetRng::New(MakeProcessRandomSeed());
    return generator;
}

} // namespace

void SetSeed(std::uint32_t seed) {
    GetRandomGenerator() = DetRng::New(seed == 0 ? 1U : seed);
}

std::uint32_t RandomU32() {
    return GetRandomGenerator().NextU32();
}

int RandomIntInclusive(int minimum, int maximum) {
    return GetRandomGenerator().RandomIntInclusive(minimum, maximum);
}

int RandomIntExclusive(int minimum, int maximum) {
    return GetRandomGenerator().RandomIntExclusive(minimum, maximum);
}

float RandomFloat(float minimum, float maximum) {
    return GetRandomGenerator().RandomFloat(minimum, maximum);
}

} // namespace rng

FVec2 GetMinDisplacement(const FAABB& aabb1, const FAABB& aabb2) {
    float dx = 0.0F;
    if (aabb1.br.x < aabb2.tl.x) {
        dx = aabb2.tl.x - aabb1.br.x;
    } else if (aabb1.tl.x > aabb2.br.x) {
        dx = aabb2.br.x - aabb1.tl.x;
    }

    float dy = 0.0F;
    if (aabb1.br.y < aabb2.tl.y) {
        dy = aabb2.tl.y - aabb1.br.y;
    } else if (aabb1.tl.y > aabb2.br.y) {
        dy = aabb2.br.y - aabb1.tl.y;
    }

    return FVec2::New(dx, dy);
}

bool AabbsIntersect(const FAABB& left, const FAABB& right) {
    if (left.br.x < right.tl.x) {
        return false;
    }
    if (left.tl.x > right.br.x) {
        return false;
    }
    if (left.br.y < right.tl.y) {
        return false;
    }
    if (left.tl.y > right.br.y) {
        return false;
    }
    return true;
}

FxAABB ToFxAABB(const FAABB& value, gfxp::Rounding rounding) {
    return FxAABB::from_corners(ToFxVec2(value.tl, rounding),
                                   ToFxVec2(value.br, rounding));
}

FAABB ToFAABB(const FxAABB& value) {
    return FAABB::New(ToFVec2(value.tl), ToFVec2(value.br));
}

IAABB ToIAABBFloorCeil(const FxAABB& value) {
    return IAABB::New(IVec2::New(value.tl.x.floor_int(), value.tl.y.floor_int()),
                      IVec2::New(value.br.x.ceil_int(), value.br.y.ceil_int()));
}

FVec2 ToFMinDisplacement(FxAABB aabb1, FxAABB aabb2) {
    return ToFVec2(gfxp::min_displacement(aabb1, aabb2));
}

} // namespace splonks

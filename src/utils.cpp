#include "utils.hpp"

#include <random>

namespace splonks {

AABB AABB::New(const Vec2& top_left, const Vec2& bottom_right) {
    AABB result;
    result.tl = top_left;
    result.br = bottom_right;
    return result;
}

IAABB AABB::AsIAABB() const {
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

AABB IAABB::AsAABB() const {
    AABB result;
    result.tl = Vec2::New(static_cast<float>(tl.x), static_cast<float>(tl.y));
    result.br = Vec2::New(static_cast<float>(br.x), static_cast<float>(br.y));
    return result;
}


namespace rng {

namespace {

std::mt19937& GetRandomGenerator() {
    static std::random_device device;
    static std::mt19937 generator(device());
    return generator;
}

} // namespace

int RandomIntInclusive(int minimum, int maximum) {
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(GetRandomGenerator());
}

int RandomIntExclusive(int minimum, int maximum) {
    std::uniform_int_distribution<int> distribution(minimum, maximum - 1);
    return distribution(GetRandomGenerator());
}

float RandomFloat(float minimum, float maximum) {
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(GetRandomGenerator());
}

} // namespace rng

Vec2 GetMinDisplacement(const AABB& aabb1, const AABB& aabb2) {
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

    return Vec2::New(dx, dy);
}

bool AabbsIntersect(const AABB& left, const AABB& right) {
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

} // namespace splonks

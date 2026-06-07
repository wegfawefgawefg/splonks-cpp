#pragma once

#include "particles/particle_specs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace splonks {

constexpr std::size_t kMaxSegmentedSpriteParticlePoints = 64;

struct SegmentedSpriteParticle {
    std::uint32_t counter = 0;
    bool finish_on_anim_end = false;
    SegmentedSpriteParticleSpecId spec_id =
        kInvalidSegmentedSpriteParticleSpecId;
    float alpha = 1.0F;
    bool horizontal_flip = false;
    std::array<Vec2, kMaxSegmentedSpriteParticlePoints> points{};
    std::uint32_t point_count = 0;
    AFrameAnimator aframe_animator{};

    void Step(const AFrameDb& aframe_db, float dt);
    bool IsFinished() const;
};

} // namespace splonks

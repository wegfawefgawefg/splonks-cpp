#pragma once

#include "particles/particle_specs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace splonks {

constexpr std::size_t kMaxRibbonParticlePoints = 32;

struct RibbonParticle {
    std::uint32_t counter = 0;
    bool finish_on_anim_end = false;
    RibbonParticleSpecId spec_id = kInvalidRibbonParticleSpecId;
    float alpha = 1.0F;
    std::array<Vec2, kMaxRibbonParticlePoints> points{};
    std::uint32_t point_count = 0;
    AFrameAnimator aframe_animator{};

    void Step(const AFrameDb& aframe_db, float dt);
    bool IsFinished() const;
};

} // namespace splonks

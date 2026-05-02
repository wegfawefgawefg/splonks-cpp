#pragma once

#include "draw_layer.hpp"
#include "frame_data_animator.hpp"
#include "math_types.hpp"
#include "particles/lighting_mode.hpp"

#include <cstdint>

namespace splonks {

struct SpriteParticle {
    std::uint32_t counter = 0;
    bool finish_on_animation_end = false;
    DrawLayer draw_layer = DrawLayer::Middle;
    ParticleLightingMode lighting_mode = ParticleLightingMode::SceneLit;
    Vec2 pos{};
    Vec2 size{};
    float rot = 0.0F;
    float alpha = 1.0F;
    float tint_r = 1.0F;
    float tint_g = 1.0F;
    float tint_b = 1.0F;
    bool horizontal_flip = false;
    Vec2 vel{};
    Vec2 svel{};
    float rotvel = 0.0F;
    float alpha_vel = 0.0F;
    Vec2 acc{};
    Vec2 sacc{};
    float rotacc = 0.0F;
    float alpha_acc = 0.0F;
    FrameDataAnimator frame_data_animator{};

    void Step(const FrameDataDb& frame_data_db, float dt);
    bool IsFinished() const;
};

} // namespace splonks

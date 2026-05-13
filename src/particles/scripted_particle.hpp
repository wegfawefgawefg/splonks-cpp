#pragma once

#include "particles/particle_specs.hpp"

namespace splonks {

struct ScriptedParticle {
    bool active = false;
    ScriptedParticleSpecId spec_id = kInvalidScriptedParticleSpecId;
    DrawLayer draw_layer = DrawLayer::Middle;
    ParticleLightingMode lighting_mode = ParticleLightingMode::SceneLit;
    Vec2 pos{};
    Vec2 size{};
    float rot = 0.0F;
    float alpha = 1.0F;
    bool horizontal_flip = false;
    std::uint32_t sequence_step_index = 0;
    std::uint32_t hold_frames_remaining = 0;
    AFrameAnimator aframe_animator{};

    void Step(const AFrameDb& aframe_db, float dt);
    bool IsFinished() const;
};

ScriptedParticle MakeScriptedParticle(
    ScriptedParticleSpecId spec_id,
    const Vec2& pos,
    bool horizontal_flip = false
);

} // namespace splonks

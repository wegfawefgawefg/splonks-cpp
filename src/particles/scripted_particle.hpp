#pragma once

#include "particles/particle_archetypes.hpp"

namespace splonks {

struct ScriptedParticle {
    bool active = false;
    ScriptedParticleArchetypeId archetype_id = kInvalidScriptedParticleArchetypeId;
    DrawLayer draw_layer = DrawLayer::Middle;
    Vec2 pos{};
    Vec2 size{};
    float rot = 0.0F;
    float alpha = 1.0F;
    bool horizontal_flip = false;
    std::uint32_t sequence_step_index = 0;
    std::uint32_t hold_frames_remaining = 0;
    FrameDataAnimator frame_data_animator{};

    void Step(const FrameDataDb& frame_data_db, float dt);
    bool IsFinished() const;
};

ScriptedParticle MakeScriptedParticle(
    ScriptedParticleArchetypeId archetype_id,
    const Vec2& pos,
    bool horizontal_flip = false
);

} // namespace splonks

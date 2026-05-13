#include "particles/scripted_particle.hpp"

namespace splonks {

namespace {

void StartSequenceStep(
    ScriptedParticle& particle,
    const ScriptedParticleSpec& spec,
    std::uint32_t sequence_step_index
) {
    if (sequence_step_index >= spec.sequence.size()) {
        if (spec.hold_frames_after_sequence > 0 && particle.hold_frames_remaining == 0) {
            particle.hold_frames_remaining = spec.hold_frames_after_sequence;
            return;
        }
        particle.active = false;
        return;
    }

    particle.sequence_step_index = sequence_step_index;
    particle.hold_frames_remaining = 0;
    const ScriptedParticleSequenceStep& step = spec.sequence[sequence_step_index];
    if (step.play_count <= 1) {
        particle.aframe_animator.Play(step.anim_id, step.playback_mode, false, 1);
        return;
    }

    particle.aframe_animator.Play(step.anim_id, step.playback_mode, false, step.play_count);
}

} // namespace

ScriptedParticle MakeScriptedParticle(
    ScriptedParticleSpecId spec_id,
    const Vec2& pos,
    bool horizontal_flip
) {
    ScriptedParticle particle;
    const ScriptedParticleSpec* const spec = GetScriptedParticleSpec(spec_id);
    if (spec == nullptr || spec->sequence.empty()) {
        return particle;
    }

    particle.active = true;
    particle.spec_id = spec_id;
    particle.draw_layer = spec->draw_layer;
    particle.lighting_mode = spec->lighting_mode;
    particle.pos = pos;
    particle.size = spec->size;
    particle.horizontal_flip = horizontal_flip;
    StartSequenceStep(particle, *spec, 0);
    return particle;
}

void ScriptedParticle::Step(const AFrameDb& aframe_db, float dt) {
    if (!active) {
        return;
    }

    const ScriptedParticleSpec* const spec = GetScriptedParticleSpec(spec_id);
    if (spec == nullptr || spec->sequence.empty()) {
        active = false;
        return;
    }

    if (hold_frames_remaining > 0) {
        hold_frames_remaining -= 1;
        if (hold_frames_remaining == 0) {
            active = false;
        }
        return;
    }

    aframe_animator.Step(aframe_db, dt);
    if (!aframe_animator.IsFinished()) {
        return;
    }

    StartSequenceStep(*this, *spec, sequence_step_index + 1);
}

bool ScriptedParticle::IsFinished() const {
    return !active;
}

} // namespace splonks

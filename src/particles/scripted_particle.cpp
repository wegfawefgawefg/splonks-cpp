#include "particles/scripted_particle.hpp"

namespace splonks {

namespace {

void StartSequenceStep(
    ScriptedParticle& particle,
    const ScriptedParticleArchetype& archetype,
    std::uint32_t sequence_step_index
) {
    if (sequence_step_index >= archetype.sequence.size()) {
        particle.active = false;
        return;
    }

    particle.sequence_step_index = sequence_step_index;
    const ScriptedParticleSequenceStep& step = archetype.sequence[sequence_step_index];
    if (step.play_count <= 1) {
        particle.frame_data_animator.Play(step.animation_id, step.playback_mode, false, 1);
        return;
    }

    particle.frame_data_animator.Play(step.animation_id, step.playback_mode, false, step.play_count);
}

} // namespace

ScriptedParticle MakeScriptedParticle(
    ScriptedParticleArchetypeId archetype_id,
    const Vec2& pos,
    bool horizontal_flip
) {
    ScriptedParticle particle;
    const ScriptedParticleArchetype* const archetype = GetScriptedParticleArchetype(archetype_id);
    if (archetype == nullptr || archetype->sequence.empty()) {
        return particle;
    }

    particle.active = true;
    particle.archetype_id = archetype_id;
    particle.draw_layer = archetype->draw_layer;
    particle.pos = pos;
    particle.size = archetype->size;
    particle.horizontal_flip = horizontal_flip;
    StartSequenceStep(particle, *archetype, 0);
    return particle;
}

void ScriptedParticle::Step(const FrameDataDb& frame_data_db, float dt) {
    if (!active) {
        return;
    }

    const ScriptedParticleArchetype* const archetype = GetScriptedParticleArchetype(archetype_id);
    if (archetype == nullptr || archetype->sequence.empty()) {
        active = false;
        return;
    }

    frame_data_animator.Step(frame_data_db, dt);
    if (!frame_data_animator.IsFinished()) {
        return;
    }

    StartSequenceStep(*this, *archetype, sequence_step_index + 1);
}

bool ScriptedParticle::IsFinished() const {
    return !active;
}

} // namespace splonks

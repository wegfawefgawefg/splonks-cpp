#pragma once

#include "audio.hpp"
#include "state.hpp"

namespace splonks {

struct WorldRayHit;

struct PositionalAudioAcoustics {
    FVec2 wrapped_source_world_pos = FVec2::New(0.0F, 0.0F);
    float source_openness = 0.0F;
    float listener_openness = 0.0F;
    float direct_open = 0.0F;
    float room_open = 0.0F;
    bool occluded = false;
    float direct_gain = 1.0F;
    bool low_pass_enabled = false;
    float low_pass_cutoff_hz = audio_filter::kMaxLowPassCutoffHz;
    float low_pass_wet = 1.0F;
    bool reverb_enabled = false;
    float reverb_wet = 0.0F;
    float reverb_feedback = 0.0F;
    float reverb_delay_ms = 80.0F;
    float reverb_low_pass_cutoff_hz = audio_filter::kMaxLowPassCutoffHz;
};

bool IsAudioOcclusionEnabled(const State& state);
void SetAudioOcclusionEnabled(State& state, bool enabled);

bool ShouldAudioRayHitCountAsOccluded(
    const State& state,
    const FVec2& listener_world_pos,
    const WorldRayHit& hit
);

PositionalAudioAcoustics ComputePositionalAudioAcoustics(
    State& state,
    const FVec2& listener_world_pos,
    const FVec2& source_world_pos
);

} // namespace splonks

#include "audio_acoustics.hpp"

#include "stage_acoustics.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks {

namespace {

float Clamp01(float value) {
    return std::clamp(value, 0.0F, 1.0F);
}

float Lerp(float a, float b, float t) {
    return a + ((b - a) * Clamp01(t));
}

float Distance(const Vec2& a, const Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt((dx * dx) + (dy * dy));
}

float GetAudioOcclusionListenerEpsilonPx(const State& state) {
    return std::max(0.0F, state.settings.audio.acoustics_occlusion_listener_epsilon_px);
}

float SampleStageOpennessAtWorldPos(State& state, const Vec2& world_pos) {
    EnsureStageAcoustics(state);

    IVec2 world_cell = ToIVec2(world_pos);
    if (!state.stage.TileCoordAtWcExists(world_cell)) {
        world_cell = state.stage.WrapWorldPos(world_cell);
    }
    if (!state.stage.TileCoordAtWcExists(world_cell)) {
        return 0.0F;
    }

    const IVec2 tile_pos = state.stage.GetTileCoordAtWc(world_cell);
    return Clamp01(GetStageTileOpenness(state, tile_pos.x, tile_pos.y));
}

} // namespace

bool IsAudioOcclusionEnabled(const State& state) {
    return state.audio_occlusion_enabled;
}

void SetAudioOcclusionEnabled(State& state, bool enabled) {
    state.audio_occlusion_enabled = enabled;
}

bool ShouldAudioRayHitCountAsOccluded(
    const State& state,
    const Vec2& listener_world_pos,
    const WorldRayHit& hit
) {
    if (hit.type != WorldRayHitType::Tile && hit.type != WorldRayHitType::StageBounds) {
        return false;
    }

    const float epsilon_px = GetAudioOcclusionListenerEpsilonPx(state);
    if (epsilon_px <= 0.0F) {
        return true;
    }

    const Vec2 hit_world = Vec2::New(static_cast<float>(hit.point.x), static_cast<float>(hit.point.y));
    return Distance(hit_world, listener_world_pos) > epsilon_px;
}

PositionalAudioAcoustics ComputePositionalAudioAcoustics(
    State& state,
    const Vec2& listener_world_pos,
    const Vec2& source_world_pos
) {
    PositionalAudioAcoustics result;
    const AudioSettings& settings = state.settings.audio;

    result.wrapped_source_world_pos = GetNearestWorldPoint(
        state.stage,
        listener_world_pos,
        source_world_pos
    );

    result.source_openness =
        SampleStageOpennessAtWorldPos(state, result.wrapped_source_world_pos);
    result.listener_openness =
        SampleStageOpennessAtWorldPos(state, listener_world_pos);
    result.direct_open = std::min(result.source_openness, result.listener_openness);

    const float listener_room_weight =
        std::clamp(settings.acoustics_listener_room_weight, 0.0F, 1.0F);
    result.room_open = Clamp01(
        (result.source_openness * (1.0F - listener_room_weight)) +
        (result.listener_openness * listener_room_weight)
    );

    const float direct_min_cutoff_hz = std::min(
        settings.acoustics_direct_min_cutoff_hz,
        settings.acoustics_direct_max_cutoff_hz
    );
    const float direct_max_cutoff_hz = std::max(
        settings.acoustics_direct_min_cutoff_hz,
        settings.acoustics_direct_max_cutoff_hz
    );
    const float reverb_min_cutoff_hz = std::min(
        settings.acoustics_reverb_min_cutoff_hz,
        settings.acoustics_reverb_max_cutoff_hz
    );
    const float reverb_max_cutoff_hz = std::max(
        settings.acoustics_reverb_min_cutoff_hz,
        settings.acoustics_reverb_max_cutoff_hz
    );

    result.direct_gain = 1.0F;
    result.low_pass_enabled = settings.acoustics_enabled;
    result.low_pass_cutoff_hz =
        Lerp(direct_min_cutoff_hz, direct_max_cutoff_hz, result.direct_open);
    result.low_pass_wet = 1.0F;

    if (settings.acoustics_enabled && IsAudioOcclusionEnabled(state)) {
        const Vec2 ray_delta = GetNearestWorldDelta(
            state.stage,
            result.wrapped_source_world_pos,
            listener_world_pos
        );
        const float ray_length =
            std::sqrt((ray_delta.x * ray_delta.x) + (ray_delta.y * ray_delta.y));
        const WorldRayHit hit = RaycastTiles(
            result.wrapped_source_world_pos,
            ray_delta,
            static_cast<int>(std::ceil(ray_length)) + 1,
            state
        );
        result.occluded = ShouldAudioRayHitCountAsOccluded(state, listener_world_pos, hit);
    }

    if (result.occluded) {
        result.direct_gain = audio_filter::ClampDirectGain(
            settings.acoustics_occluded_direct_gain
        );
        result.low_pass_cutoff_hz = std::min(
            result.low_pass_cutoff_hz,
            settings.acoustics_occluded_cutoff_hz
        );
    }

    result.reverb_enabled =
        settings.acoustics_enabled && settings.acoustics_reverb_enabled;
    result.reverb_wet = result.reverb_enabled
        ? Clamp01(result.room_open * settings.acoustics_reverb_send)
        : 0.0F;
    result.reverb_feedback = result.reverb_enabled
        ? Lerp(0.15F, settings.acoustics_reverb_feedback, result.room_open)
        : 0.0F;
    result.reverb_delay_ms = settings.acoustics_reverb_delay_ms;
    result.reverb_low_pass_cutoff_hz = Lerp(
        reverb_min_cutoff_hz,
        reverb_max_cutoff_hz,
        result.room_open
    );

    return result;
}

} // namespace splonks

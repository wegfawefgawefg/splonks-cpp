#pragma once

#include "frame_data.hpp"

#include <cstdint>

namespace splonks {

enum class AnimationPlaybackMode : std::uint8_t {
    Forward,
    Reverse,
    PingPong,
};

struct FrameDataAnimator {
    FrameDataId animation_id = kInvalidFrameDataId;
    std::size_t current_frame = 0;
    float current_time = 0.0F;
    float scale = 1.0F;
    float speed = 1.0F;
    bool animate = true;
    bool loop = true;
    bool finished = false;
    AnimationPlaybackMode playback_mode = AnimationPlaybackMode::Forward;
    std::uint32_t play_count = 1;
    std::uint32_t plays_completed = 0;
    bool playback_dirty = true;
    bool ping_pong_forward = true;

    static FrameDataAnimator New(FrameDataId animation_id_value);

    bool HasAnimation() const;
    bool IsFinished() const;
    // Raw animation control path.
    // Use this when entity-owned logic knows the exact authored animation id it wants.
    void SetAnimation(FrameDataId animation_id_value);
    void SetForcedFrame(std::size_t frame_index);
    void SetSpeed(float speed_value);
    void ResetSpeed();
    void SetPlaybackMode(AnimationPlaybackMode playback_mode_value);
    void SetPlayCount(std::uint32_t play_count_value);
    void Play(
        FrameDataId animation_id_value,
        AnimationPlaybackMode playback_mode_value = AnimationPlaybackMode::Forward,
        bool loop_value = true,
        std::uint32_t play_count_value = 1
    );
    void PlayLoop(
        FrameDataId animation_id_value,
        AnimationPlaybackMode playback_mode_value = AnimationPlaybackMode::Forward
    );
    void PlayOnce(FrameDataId animation_id_value);
    void PlayOnceReverse(FrameDataId animation_id_value);
    void PlayOncePingPong(FrameDataId animation_id_value);
    void PlayNTimes(
        FrameDataId animation_id_value,
        std::uint32_t play_count_value,
        AnimationPlaybackMode playback_mode_value = AnimationPlaybackMode::Forward
    );
    void Step(const FrameDataDb& frame_data_db, float dt);
};

} // namespace splonks

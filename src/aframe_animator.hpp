#pragma once

#include "aframe.hpp"

#include <cstdint>

namespace splonks {

enum class AnimPlaybackMode : std::uint8_t {
    Forward,
    Reverse,
    PingPong,
};

struct AFrameAnimator {
    AFrameId anim_id = kInvalidAFrameId;
    std::size_t current_frame = 0;
    float current_time = 0.0F;
    float scale = 1.0F;
    float speed = 1.0F;
    bool animate = true;
    bool loop = true;
    bool finished = false;
    AnimPlaybackMode playback_mode = AnimPlaybackMode::Forward;
    std::uint32_t play_count = 1;
    std::uint32_t plays_completed = 0;
    bool playback_dirty = true;
    bool ping_pong_forward = true;

    static AFrameAnimator New(AFrameId anim_id_value);

    bool HasAnim() const;
    bool IsFinished() const;
    // Raw anim control path.
    // Use this when ent-owned logic knows the exact authored anim id it wants.
    void SetAnim(AFrameId anim_id_value);
    void SetForcedFrame(std::size_t frame_index);
    void SetSpeed(float speed_value);
    void ResetSpeed();
    void SetPlaybackMode(AnimPlaybackMode playback_mode_value);
    void SetPlayCount(std::uint32_t play_count_value);
    void Play(
        AFrameId anim_id_value,
        AnimPlaybackMode playback_mode_value = AnimPlaybackMode::Forward,
        bool loop_value = true,
        std::uint32_t play_count_value = 1
    );
    void PlayLoop(
        AFrameId anim_id_value,
        AnimPlaybackMode playback_mode_value = AnimPlaybackMode::Forward
    );
    void PlayOnce(AFrameId anim_id_value);
    void PlayOnceReverse(AFrameId anim_id_value);
    void PlayOncePingPong(AFrameId anim_id_value);
    void PlayNTimes(
        AFrameId anim_id_value,
        std::uint32_t play_count_value,
        AnimPlaybackMode playback_mode_value = AnimPlaybackMode::Forward
    );
    void Step(const AFrameDb& aframe_db, float dt);
};

} // namespace splonks

#include "frame_data_animator.hpp"

#include <algorithm>

namespace splonks {

namespace {

void ResetPlaybackState(FrameDataAnimator& animator) {
    animator.finished = false;
    animator.plays_completed = 0;
    animator.playback_dirty = true;
    animator.ping_pong_forward = true;
}

std::uint32_t SanitizePlayCount(std::uint32_t play_count_value) {
    return std::max(1u, play_count_value);
}

void InitializePlayback(
    FrameDataAnimator& animator,
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db
) {
    if (!animator.playback_dirty) {
        return;
    }

    animator.finished = false;
    animator.playback_dirty = false;
    animator.ping_pong_forward = true;

    if (animation.frame_indices.empty()) {
        animator.current_frame = 0;
        animator.current_time = 0.0F;
        return;
    }

    switch (animator.playback_mode) {
    case AnimationPlaybackMode::Forward:
    case AnimationPlaybackMode::PingPong:
        animator.current_frame = 0;
        animator.current_time = 0.0F;
        break;
    case AnimationPlaybackMode::Reverse:
        animator.current_frame = animation.frame_indices.size() - 1;
        animator.current_time = static_cast<float>(
            frame_data_db.frames[animation.frame_indices[animator.current_frame]].duration
        );
        break;
    }
}

void RestartPlayback(
    FrameDataAnimator& animator,
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db
) {
    animator.playback_dirty = true;
    InitializePlayback(animator, animation, frame_data_db);
}

bool FinishOrRestart(
    FrameDataAnimator& animator,
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db,
    float finished_time
) {
    if (animator.loop) {
        RestartPlayback(animator, animation, frame_data_db);
        return true;
    }

    animator.plays_completed += 1;
    if (animator.plays_completed < animator.play_count) {
        RestartPlayback(animator, animation, frame_data_db);
        return true;
    }

    animator.current_time = finished_time;
    animator.finished = true;
    return false;
}

void StepForward(
    FrameDataAnimator& animator,
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db
) {
    const FrameData& frame_data =
        frame_data_db.frames[animation.frame_indices[animator.current_frame]];
    animator.current_time += animator.speed;
    if (animator.current_time < static_cast<float>(frame_data.duration)) {
        return;
    }

    if (animator.current_frame + 1 < animation.frame_indices.size()) {
        animator.current_frame += 1;
        animator.current_time = 0.0F;
        return;
    }

    (void)FinishOrRestart(
        animator,
        animation,
        frame_data_db,
        static_cast<float>(frame_data.duration)
    );
}

void StepReverse(
    FrameDataAnimator& animator,
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db
) {
    animator.current_time -= animator.speed;
    if (animator.current_time > 0.0F) {
        return;
    }

    if (animator.current_frame > 0) {
        animator.current_frame -= 1;
        const FrameData& frame_data =
            frame_data_db.frames[animation.frame_indices[animator.current_frame]];
        animator.current_time = static_cast<float>(frame_data.duration);
        return;
    }

    (void)FinishOrRestart(animator, animation, frame_data_db, 0.0F);
}

void StepPingPong(
    FrameDataAnimator& animator,
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db
) {
    if (animation.frame_indices.size() <= 1) {
        StepForward(animator, animation, frame_data_db);
        return;
    }

    if (animator.ping_pong_forward) {
        const FrameData& frame_data =
            frame_data_db.frames[animation.frame_indices[animator.current_frame]];
        animator.current_time += animator.speed;
        if (animator.current_time < static_cast<float>(frame_data.duration)) {
            return;
        }

        if (animator.current_frame + 1 < animation.frame_indices.size()) {
            animator.current_frame += 1;
            animator.current_time = 0.0F;
            return;
        }

        animator.ping_pong_forward = false;
        animator.current_time = static_cast<float>(frame_data.duration);
        return;
    }

    animator.current_time -= animator.speed;
    if (animator.current_time > 0.0F) {
        return;
    }

    if (animator.current_frame > 0) {
        animator.current_frame -= 1;
        const FrameData& frame_data =
            frame_data_db.frames[animation.frame_indices[animator.current_frame]];
        animator.current_time = static_cast<float>(frame_data.duration);
        return;
    }

    if (animator.loop) {
        RestartPlayback(animator, animation, frame_data_db);
        return;
    }

    animator.plays_completed += 1;
    if (animator.plays_completed < animator.play_count) {
        RestartPlayback(animator, animation, frame_data_db);
        return;
    }

    animator.current_time = 0.0F;
    animator.finished = true;
}

} // namespace

FrameDataAnimator FrameDataAnimator::New(FrameDataId animation_id_value) {
    FrameDataAnimator result;
    result.animation_id = animation_id_value;
    return result;
}

bool FrameDataAnimator::HasAnimation() const {
    return animation_id != kInvalidFrameDataId;
}

bool FrameDataAnimator::IsFinished() const {
    return finished;
}

void FrameDataAnimator::SetAnimation(FrameDataId animation_id_value) {
    if (animation_id == animation_id_value) {
        return;
    }

    current_frame = 0;
    current_time = 0.0F;
    animation_id = animation_id_value;
    ResetPlaybackState(*this);
}

void FrameDataAnimator::SetForcedFrame(std::size_t frame_index) {
    current_frame = frame_index;
    current_time = 0.0F;
    finished = false;
    playback_dirty = false;
    ping_pong_forward = playback_mode != AnimationPlaybackMode::Reverse;
}

void FrameDataAnimator::SetSpeed(float speed_value) {
    speed = std::clamp(speed_value, 0.01F, 10.0F);
}

void FrameDataAnimator::ResetSpeed() {
    speed = 1.0F;
}

void FrameDataAnimator::SetPlaybackMode(AnimationPlaybackMode playback_mode_value) {
    playback_mode = playback_mode_value;
    ResetPlaybackState(*this);
}

void FrameDataAnimator::SetPlayCount(std::uint32_t play_count_value) {
    play_count = SanitizePlayCount(play_count_value);
    ResetPlaybackState(*this);
}

void FrameDataAnimator::Play(
    FrameDataId animation_id_value,
    AnimationPlaybackMode playback_mode_value,
    bool loop_value,
    std::uint32_t play_count_value
) {
    animation_id = animation_id_value;
    current_frame = 0;
    current_time = 0.0F;
    animate = true;
    loop = loop_value;
    playback_mode = playback_mode_value;
    play_count = SanitizePlayCount(play_count_value);
    ResetPlaybackState(*this);
}

void FrameDataAnimator::PlayLoop(
    FrameDataId animation_id_value,
    AnimationPlaybackMode playback_mode_value
) {
    Play(animation_id_value, playback_mode_value, true, 1);
}

void FrameDataAnimator::PlayOnce(FrameDataId animation_id_value) {
    Play(animation_id_value, AnimationPlaybackMode::Forward, false, 1);
}

void FrameDataAnimator::PlayOnceReverse(FrameDataId animation_id_value) {
    Play(animation_id_value, AnimationPlaybackMode::Reverse, false, 1);
}

void FrameDataAnimator::PlayOncePingPong(FrameDataId animation_id_value) {
    Play(animation_id_value, AnimationPlaybackMode::PingPong, false, 1);
}

void FrameDataAnimator::PlayNTimes(
    FrameDataId animation_id_value,
    std::uint32_t play_count_value,
    AnimationPlaybackMode playback_mode_value
) {
    Play(animation_id_value, playback_mode_value, false, play_count_value);
}

void FrameDataAnimator::Step(const FrameDataDb& frame_data_db, float dt) {
    (void)dt;
    if (!animate || animation_id == kInvalidFrameDataId || finished) {
        return;
    }

    const FrameDataAnimation* const animation = frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return;
    }

    InitializePlayback(*this, *animation, frame_data_db);
    if (finished) {
        return;
    }

    switch (playback_mode) {
    case AnimationPlaybackMode::Forward:
        StepForward(*this, *animation, frame_data_db);
        break;
    case AnimationPlaybackMode::Reverse:
        StepReverse(*this, *animation, frame_data_db);
        break;
    case AnimationPlaybackMode::PingPong:
        StepPingPong(*this, *animation, frame_data_db);
        break;
    }
}

} // namespace splonks

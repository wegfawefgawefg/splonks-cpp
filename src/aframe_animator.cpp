#include "aframe_animator.hpp"

#include <algorithm>

namespace splonks {

namespace {

void ResetPlaybackState(AFrameAnimator& animator) {
    animator.finished = false;
    animator.plays_completed = 0;
    animator.playback_dirty = true;
    animator.ping_pong_forward = true;
}

std::uint32_t SanitizePlayCount(std::uint32_t play_count_value) {
    return std::max(1u, play_count_value);
}

sim::Scalar DurationScalar(const AFrame& aframe) {
    return sim::Scalar::from_int(static_cast<std::int32_t>(aframe.duration));
}

void InitializePlayback(
    AFrameAnimator& animator,
    const AFrameAnim& anim,
    const AFrameDb& aframe_db
) {
    if (!animator.playback_dirty) {
        return;
    }

    animator.finished = false;
    animator.playback_dirty = false;
    animator.ping_pong_forward = true;

    if (anim.frame_indices.empty()) {
        animator.current_frame = 0;
        animator.current_time = sim::Scalar::zero();
        return;
    }

    switch (animator.playback_mode) {
    case AnimPlaybackMode::Forward:
    case AnimPlaybackMode::PingPong:
        animator.current_frame = 0;
        animator.current_time = sim::Scalar::zero();
        break;
    case AnimPlaybackMode::Reverse:
        animator.current_frame = static_cast<std::uint32_t>(anim.frame_indices.size() - 1);
        animator.current_time = DurationScalar(
            aframe_db.frames[anim.frame_indices[static_cast<std::size_t>(animator.current_frame)]]
        );
        break;
    }
}

void RestartPlayback(
    AFrameAnimator& animator,
    const AFrameAnim& anim,
    const AFrameDb& aframe_db
) {
    animator.playback_dirty = true;
    InitializePlayback(animator, anim, aframe_db);
}

bool FinishOrRestart(
    AFrameAnimator& animator,
    const AFrameAnim& anim,
    const AFrameDb& aframe_db,
    sim::Scalar finished_time
) {
    if (animator.loop) {
        RestartPlayback(animator, anim, aframe_db);
        return true;
    }

    animator.plays_completed += 1;
    if (animator.plays_completed < animator.play_count) {
        RestartPlayback(animator, anim, aframe_db);
        return true;
    }

    animator.current_time = finished_time;
    animator.finished = true;
    return false;
}

void StepForward(
    AFrameAnimator& animator,
    const AFrameAnim& anim,
    const AFrameDb& aframe_db
) {
    const AFrame& aframe =
        aframe_db.frames[anim.frame_indices[static_cast<std::size_t>(animator.current_frame)]];
    animator.current_time += animator.speed;
    if (animator.current_time < DurationScalar(aframe)) {
        return;
    }

    if (static_cast<std::size_t>(animator.current_frame) + 1 < anim.frame_indices.size()) {
        animator.current_frame += 1;
        animator.current_time = sim::Scalar::zero();
        return;
    }

    (void)FinishOrRestart(
        animator,
        anim,
        aframe_db,
        DurationScalar(aframe)
    );
}

void StepReverse(
    AFrameAnimator& animator,
    const AFrameAnim& anim,
    const AFrameDb& aframe_db
) {
    animator.current_time -= animator.speed;
    if (animator.current_time > sim::Scalar::zero()) {
        return;
    }

    if (animator.current_frame > 0) {
        animator.current_frame -= 1;
        const AFrame& aframe =
            aframe_db.frames[anim.frame_indices[static_cast<std::size_t>(animator.current_frame)]];
        animator.current_time = DurationScalar(aframe);
        return;
    }

    (void)FinishOrRestart(animator, anim, aframe_db, sim::Scalar::zero());
}

void StepPingPong(
    AFrameAnimator& animator,
    const AFrameAnim& anim,
    const AFrameDb& aframe_db
) {
    if (anim.frame_indices.size() <= 1) {
        StepForward(animator, anim, aframe_db);
        return;
    }

    if (animator.ping_pong_forward) {
        const AFrame& aframe =
            aframe_db.frames[anim.frame_indices[static_cast<std::size_t>(animator.current_frame)]];
        animator.current_time += animator.speed;
        if (animator.current_time < DurationScalar(aframe)) {
            return;
        }

        if (static_cast<std::size_t>(animator.current_frame) + 1 < anim.frame_indices.size()) {
            animator.current_frame += 1;
            animator.current_time = sim::Scalar::zero();
            return;
        }

        animator.ping_pong_forward = false;
        animator.current_time = DurationScalar(aframe);
        return;
    }

    animator.current_time -= animator.speed;
    if (animator.current_time > sim::Scalar::zero()) {
        return;
    }

    if (animator.current_frame > 0) {
        animator.current_frame -= 1;
        const AFrame& aframe =
            aframe_db.frames[anim.frame_indices[static_cast<std::size_t>(animator.current_frame)]];
        animator.current_time = DurationScalar(aframe);
        return;
    }

    if (animator.loop) {
        RestartPlayback(animator, anim, aframe_db);
        return;
    }

    animator.plays_completed += 1;
    if (animator.plays_completed < animator.play_count) {
        RestartPlayback(animator, anim, aframe_db);
        return;
    }

    animator.current_time = sim::Scalar::zero();
    animator.finished = true;
}

} // namespace

AFrameAnimator AFrameAnimator::New(AFrameId anim_id_value) {
    AFrameAnimator result;
    result.anim_id = anim_id_value;
    return result;
}

bool AFrameAnimator::HasAnim() const {
    return anim_id != kInvalidAFrameId;
}

bool AFrameAnimator::IsFinished() const {
    return finished;
}

void AFrameAnimator::SetAnim(AFrameId anim_id_value) {
    if (anim_id == anim_id_value) {
        return;
    }

    current_frame = 0;
    current_time = sim::Scalar::zero();
    anim_id = anim_id_value;
    ResetPlaybackState(*this);
}

void AFrameAnimator::SetForcedFrame(std::uint32_t frame_index) {
    current_frame = frame_index;
    current_time = sim::Scalar::zero();
    finished = false;
    playback_dirty = false;
    ping_pong_forward = playback_mode != AnimPlaybackMode::Reverse;
}

void AFrameAnimator::SetSpeed(float speed_value) {
    speed = sim::ToSimScalar(std::clamp(speed_value, 0.01F, 10.0F));
}

void AFrameAnimator::ResetSpeed() {
    speed = sim::Scalar::from_int(1);
}

void AFrameAnimator::SetPlaybackMode(AnimPlaybackMode playback_mode_value) {
    playback_mode = playback_mode_value;
    ResetPlaybackState(*this);
}

void AFrameAnimator::SetPlayCount(std::uint32_t play_count_value) {
    play_count = SanitizePlayCount(play_count_value);
    ResetPlaybackState(*this);
}

void AFrameAnimator::Play(
    AFrameId anim_id_value,
    AnimPlaybackMode playback_mode_value,
    bool loop_value,
    std::uint32_t play_count_value
) {
    anim_id = anim_id_value;
    current_frame = 0;
    current_time = sim::Scalar::zero();
    animate = true;
    loop = loop_value;
    playback_mode = playback_mode_value;
    play_count = SanitizePlayCount(play_count_value);
    ResetPlaybackState(*this);
}

void AFrameAnimator::PlayLoop(
    AFrameId anim_id_value,
    AnimPlaybackMode playback_mode_value
) {
    Play(anim_id_value, playback_mode_value, true, 1);
}

void AFrameAnimator::PlayOnce(AFrameId anim_id_value) {
    Play(anim_id_value, AnimPlaybackMode::Forward, false, 1);
}

void AFrameAnimator::PlayOnceReverse(AFrameId anim_id_value) {
    Play(anim_id_value, AnimPlaybackMode::Reverse, false, 1);
}

void AFrameAnimator::PlayOncePingPong(AFrameId anim_id_value) {
    Play(anim_id_value, AnimPlaybackMode::PingPong, false, 1);
}

void AFrameAnimator::PlayNTimes(
    AFrameId anim_id_value,
    std::uint32_t play_count_value,
    AnimPlaybackMode playback_mode_value
) {
    Play(anim_id_value, playback_mode_value, false, play_count_value);
}

void AFrameAnimator::Step(const AFrameDb& aframe_db, float dt) {
    (void)dt;
    if (!animate || anim_id == kInvalidAFrameId || finished) {
        return;
    }

    const AFrameAnim* const anim = aframe_db.FindAnim(anim_id);
    if (anim == nullptr || anim->frame_indices.empty()) {
        return;
    }

    InitializePlayback(*this, *anim, aframe_db);
    if (finished) {
        return;
    }

    switch (playback_mode) {
    case AnimPlaybackMode::Forward:
        StepForward(*this, *anim, aframe_db);
        break;
    case AnimPlaybackMode::Reverse:
        StepReverse(*this, *anim, aframe_db);
        break;
    case AnimPlaybackMode::PingPong:
        StepPingPong(*this, *anim, aframe_db);
        break;
    }
}

} // namespace splonks

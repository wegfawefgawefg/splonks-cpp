#include "frame_data_animator.hpp"

#include <algorithm>

namespace splonks {

FrameDataAnimator FrameDataAnimator::New(FrameDataId animation_id_value) {
    FrameDataAnimator result;
    result.animation_id = animation_id_value;
    return result;
}

bool FrameDataAnimator::HasAnimation() const {
    return animation_id != kInvalidFrameDataId;
}

void FrameDataAnimator::SetAnimation(FrameDataId animation_id_value) {
    if (animation_id != animation_id_value) {
        current_frame = 0;
        current_time = 0.0F;
    }
    animation_id = animation_id_value;
}

void FrameDataAnimator::SetForcedFrame(std::size_t frame_index) {
    current_frame = frame_index;
    current_time = 0.0F;
}

void FrameDataAnimator::SetSpeed(float speed_value) {
    speed = std::clamp(speed_value, 0.01F, 10.0F);
}

void FrameDataAnimator::ResetSpeed() {
    speed = 1.0F;
}

void FrameDataAnimator::Step(const FrameDataDb& frame_data_db, float dt) {
    (void)dt;
    if (!animate || animation_id == kInvalidFrameDataId) {
        return;
    }

    const FrameDataAnimation* const animation = frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return;
    }

    if (current_frame >= animation->frame_indices.size()) {
        current_frame = 0;
        current_time = 0.0F;
    }

    const FrameData& frame_data = frame_data_db.frames[animation->frame_indices[current_frame]];
    current_time += speed;
    if (current_time >= static_cast<float>(frame_data.duration)) {
        current_time = 0.0F;
        current_frame = (current_frame + 1) % animation->frame_indices.size();
    }
}

} // namespace splonks

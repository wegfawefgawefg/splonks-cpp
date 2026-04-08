#pragma once

#include "frame_data.hpp"

namespace splonks {

struct FrameDataAnimator {
    FrameDataId animation_id = kInvalidFrameDataId;
    std::size_t current_frame = 0;
    float current_time = 0.0F;
    float scale = 1.0F;
    float speed = 1.0F;
    bool animate = true;
    bool loop = true;
    bool finished = false;

    static FrameDataAnimator New(FrameDataId animation_id_value);

    bool HasAnimation() const;
    bool IsFinished() const;
    void SetAnimation(FrameDataId animation_id_value);
    void SetForcedFrame(std::size_t frame_index);
    void SetSpeed(float speed_value);
    void ResetSpeed();
    void Step(const FrameDataDb& frame_data_db, float dt);
};

} // namespace splonks

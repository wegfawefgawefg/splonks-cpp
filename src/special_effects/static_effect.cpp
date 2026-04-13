#include "special_effects/static_effect.hpp"

namespace splonks {

void StaticEffect::Step(const FrameDataDb& frame_data_db, float dt) {
    if (counter > 0) {
        counter -= 1;
    }

    frame_data_animator.Step(frame_data_db, dt);
}

bool StaticEffect::IsFinished() const {
    return counter == 0 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

Vec2 StaticEffect::GetPos() const {
    return pos;
}

Vec2 StaticEffect::GetSize() const {
    return size;
}

float StaticEffect::GetRot() const {
    return rot;
}

float StaticEffect::GetAlpha() const {
    return alpha;
}

const FrameDataAnimator& StaticEffect::GetFrameDataAnimator() const {
    return frame_data_animator;
}

} // namespace splonks

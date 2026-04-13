#include "special_effects/dynamic_effect.hpp"

namespace splonks {

void DynamicEffect::Step(const FrameDataDb& frame_data_db, float dt) {
    if (counter > 0) {
        counter -= 1;
    }

    frame_data_animator.Step(frame_data_db, dt);
    pos += vel;
    size += svel;
    rot += rotvel;
    alpha += alpha_vel;

    size = Max(size, Vec2::New(0.0F, 0.0F));
    alpha = Max(Min(alpha, 1.0F), 0.0F);
}

bool DynamicEffect::IsFinished() const {
    return counter == 0 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

Vec2 DynamicEffect::GetPos() const {
    return pos;
}

Vec2 DynamicEffect::GetSize() const {
    return size;
}

float DynamicEffect::GetRot() const {
    return rot;
}

float DynamicEffect::GetAlpha() const {
    return alpha;
}

const FrameDataAnimator& DynamicEffect::GetFrameDataAnimator() const {
    return frame_data_animator;
}

} // namespace splonks

#include "special_effects/ultra_dynamic_effect.hpp"

namespace splonks {

void UltraDynamicEffect::Step(const FrameDataDb& frame_data_db, float dt) {
    if (counter > 0) {
        counter -= 1;
    }

    frame_data_animator.Step(frame_data_db, dt);
    vel += acc;
    svel += sacc;
    rotvel += rotacc;
    alpha_vel += alpha_acc;

    pos += vel;
    size += svel;
    rot += rotvel;
    alpha += alpha_vel;

    size = Max(size, Vec2::New(0.0F, 0.0F));
    alpha = Max(Min(alpha, 1.0F), 0.0F);
}

bool UltraDynamicEffect::IsFinished() const {
    return counter == 0 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

Vec2 UltraDynamicEffect::GetPos() const {
    return pos;
}

Vec2 UltraDynamicEffect::GetSize() const {
    return size;
}

float UltraDynamicEffect::GetRot() const {
    return rot;
}

float UltraDynamicEffect::GetAlpha() const {
    return alpha;
}

const FrameDataAnimator& UltraDynamicEffect::GetFrameDataAnimator() const {
    return frame_data_animator;
}

} // namespace splonks

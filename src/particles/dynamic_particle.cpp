#include "particles/dynamic_particle.hpp"

namespace splonks {

void DynamicParticle::Step(const FrameDataDb& frame_data_db, float dt) {
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

bool DynamicParticle::IsFinished() const {
    return counter == 0 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

Vec2 DynamicParticle::GetPos() const {
    return pos;
}

Vec2 DynamicParticle::GetSize() const {
    return size;
}

float DynamicParticle::GetRot() const {
    return rot;
}

float DynamicParticle::GetAlpha() const {
    return alpha;
}

const FrameDataAnimator& DynamicParticle::GetFrameDataAnimator() const {
    return frame_data_animator;
}

} // namespace splonks

#include "particles/ultra_dynamic_particle.hpp"

namespace splonks {

void UltraDynamicParticle::Step(const FrameDataDb& frame_data_db, float dt) {
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

bool UltraDynamicParticle::IsFinished() const {
    return counter == 0 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

Vec2 UltraDynamicParticle::GetPos() const {
    return pos;
}

Vec2 UltraDynamicParticle::GetSize() const {
    return size;
}

float UltraDynamicParticle::GetRot() const {
    return rot;
}

float UltraDynamicParticle::GetAlpha() const {
    return alpha;
}

const FrameDataAnimator& UltraDynamicParticle::GetFrameDataAnimator() const {
    return frame_data_animator;
}

} // namespace splonks

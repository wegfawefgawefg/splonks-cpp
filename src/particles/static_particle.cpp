#include "particles/static_particle.hpp"

namespace splonks {

void StaticParticle::Step(const FrameDataDb& frame_data_db, float dt) {
    if (counter > 0) {
        counter -= 1;
    }

    frame_data_animator.Step(frame_data_db, dt);
}

bool StaticParticle::IsFinished() const {
    return counter == 0 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

Vec2 StaticParticle::GetPos() const {
    return pos;
}

Vec2 StaticParticle::GetSize() const {
    return size;
}

float StaticParticle::GetRot() const {
    return rot;
}

float StaticParticle::GetAlpha() const {
    return alpha;
}

const FrameDataAnimator& StaticParticle::GetFrameDataAnimator() const {
    return frame_data_animator;
}

} // namespace splonks

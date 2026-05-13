#include "particles/ribbon_particle.hpp"

namespace splonks {

void RibbonParticle::Step(const AFrameDb& aframe_db, float dt) {
    if (counter > 0) {
        counter -= 1;
    }
    aframe_animator.Step(aframe_db, dt);
    alpha = Max(Min(alpha, 1.0F), 0.0F);
}

bool RibbonParticle::IsFinished() const {
    return counter == 0 || point_count < 2 || (finish_on_anim_end && aframe_animator.IsFinished());
}

} // namespace splonks

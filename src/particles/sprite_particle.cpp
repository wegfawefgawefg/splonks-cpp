#include "particles/sprite_particle.hpp"

namespace splonks {

void SpriteParticle::Step(const AFrameDb& aframe_db, float dt) {
    (void)dt;
    if (counter > 0) {
        counter -= 1;
    }

    aframe_animator.Step(aframe_db, dt);
    vel += acc;
    svel += sacc;
    rotvel += rotacc;
    alpha_vel += alpha_acc;

    pos += vel;
    size += svel;
    rot += rotvel;
    alpha += alpha_vel;

    size = Max(size, FVec2::New(0.0F, 0.0F));
    alpha = Max(Min(alpha, 1.0F), 0.0F);
}

bool SpriteParticle::IsFinished() const {
    return counter == 0 || (finish_on_anim_end && aframe_animator.IsFinished());
}

} // namespace splonks

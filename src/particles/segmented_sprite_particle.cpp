#include "particles/segmented_sprite_particle.hpp"

namespace splonks {

void SegmentedSpriteParticle::Step(const FrameDataDb& frame_data_db, float dt) {
    if (counter > 0) {
        counter -= 1;
    }
    frame_data_animator.Step(frame_data_db, dt);
    alpha = Max(Min(alpha, 1.0F), 0.0F);
}

bool SegmentedSpriteParticle::IsFinished() const {
    return counter == 0 || point_count < 2 || (finish_on_animation_end && frame_data_animator.IsFinished());
}

} // namespace splonks

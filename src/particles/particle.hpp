#pragma once

#include "frame_data_animator.hpp"
#include "math_types.hpp"

namespace splonks {

class Particle {
  public:
    virtual ~Particle() = default;

    virtual void Step(const FrameDataDb& frame_data_db, float dt) = 0;
    virtual bool IsFinished() const = 0;

    virtual Vec2 GetPos() const = 0;
    virtual Vec2 GetSize() const = 0;
    virtual float GetRot() const = 0;
    virtual float GetAlpha() const = 0;
    virtual const FrameDataAnimator& GetFrameDataAnimator() const = 0;
};

} // namespace splonks

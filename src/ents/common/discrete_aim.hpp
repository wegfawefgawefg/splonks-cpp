#pragma once

#include "controls.hpp"
#include "ent.hpp"
#include "fxp.hpp"
#include "state.hpp"

namespace splonks::ents::common {

struct DiscreteHeldWeaponAim {
    FxVec2 direction = FxVec2{FxScalar::from_int(1), FxScalar::zero()};
    Side facing = Side::Right;
    FxScalar rotation = FxScalar::zero();
};

inline float NormalizeDegrees(float degrees) {
    while (degrees > 180.0F) {
        degrees -= 360.0F;
    }
    while (degrees <= -180.0F) {
        degrees += 360.0F;
    }
    return degrees;
}

inline FxVec2 DiscreteAimDirection(int aim_x, int aim_y, Side facing) {
    constexpr float kDiagonalAimComponent = 0.707106769F;
    if (aim_x == 0 && aim_y == 0) {
        return FxVec2{
            FxScalar::from_int(facing == Side::Left ? -1 : 1),
            FxScalar::zero(),
        };
    }
    if (aim_x != 0 && aim_y != 0) {
        return FxVec2{
            FxScalar::from_int(aim_x) * ToFxScalar(kDiagonalAimComponent),
            FxScalar::from_int(aim_y) * ToFxScalar(kDiagonalAimComponent),
        };
    }
    return FxVec2{
        FxScalar::from_int(aim_x),
        FxScalar::from_int(aim_y),
    };
}

inline float DiscreteAimWorldAngle(int aim_x, int aim_y, Side facing) {
    if (aim_x == 0 && aim_y == 0) {
        return facing == Side::Left ? 180.0F : 0.0F;
    }
    if (aim_x > 0) {
        if (aim_y < 0) {
            return -45.0F;
        }
        if (aim_y > 0) {
            return 45.0F;
        }
        return 0.0F;
    }
    if (aim_x < 0) {
        if (aim_y < 0) {
            return -135.0F;
        }
        if (aim_y > 0) {
            return 135.0F;
        }
        return 180.0F;
    }
    return aim_y < 0 ? -90.0F : 90.0F;
}

inline DiscreteHeldWeaponAim GetDiscreteHeldWeaponAim(
    const Ent& weapon,
    const Ent* holder,
    const State& state
) {
    int aim_x = 0;
    int aim_y = 0;
    Side facing = holder != nullptr ? holder->facing : weapon.facing;
    if (holder != nullptr) {
        const controls::ControlIntent intent = controls::GetControlIntentForEnt(*holder, state);
        if (intent.left && !intent.right) {
            aim_x = -1;
        } else if (intent.right && !intent.left) {
            aim_x = 1;
        }
        if (intent.up && !intent.down) {
            aim_y = -1;
        } else if (intent.down && !intent.up) {
            aim_y = 1;
        }
    }

    if (aim_x < 0) {
        facing = Side::Left;
    } else if (aim_x > 0) {
        facing = Side::Right;
    }

    const FxVec2 direction = DiscreteAimDirection(aim_x, aim_y, facing);
    const float world_angle = DiscreteAimWorldAngle(aim_x, aim_y, facing);
    const float base_angle = facing == Side::Left ? 180.0F : 0.0F;
    return DiscreteHeldWeaponAim{
        .direction = direction,
        .facing = facing,
        .rotation = ToFxScalar(NormalizeDegrees(world_angle - base_angle)),
    };
}

} // namespace splonks::ents::common

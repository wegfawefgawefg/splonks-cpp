#pragma once

#include <gubsy/input/binds_profile.hpp>

namespace splonks {

enum SplonksGubsyAction {
    kGubsyActionMenuUp = 0,
    kGubsyActionMenuDown = 1,
    kGubsyActionMenuLeft = 2,
    kGubsyActionMenuRight = 3,
    kGubsyActionConfirmJump = 4,
    kGubsyActionBackAttack = 5,
    kGubsyActionPagePreviousBomb = 6,
    kGubsyActionPageNextRope = 7,
    kGubsyActionMoveUp = 8,
    kGubsyActionMoveDown = 9,
    kGubsyActionMoveLeft = 10,
    kGubsyActionMoveRight = 11,
    kGubsyActionUse = 12,
};

BindsSchema BuildGubsyBindsSchema();

} // namespace splonks

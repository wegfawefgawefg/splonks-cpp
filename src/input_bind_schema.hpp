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
    kGubsyActionRun = 12,
    kGubsyActionUse = 13,
    kGubsyActionUseBack = 14,
    kGubsyActionEquip = 15,
    kGubsyActionPickUpDrop = 16,
    kGubsyActionStopNextStage = 17,
    kGubsyActionBombGrenade = 18,
    kGubsyActionRope = 19,
    kGubsyActionAttack = 20,
    kGubsyActionBuy = 21,
    kGubsyActionEmoteUp = 22,
    kGubsyActionEmoteDown = 23,
};

enum SplonksGubsyAxis1D {
    kGubsyAxisRun = 0,
    kGubsyAxisUseBack = 1,
};

BindsSchema BuildGubsyBindsSchema();

} // namespace splonks

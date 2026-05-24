#include "input_bind_schema.hpp"

namespace splonks {

BindsSchema BuildGubsyBindsSchema() {
    BindsSchema schema;
    schema.add_action(kGubsyActionMenuUp, "Menu Up", "Menu");
    schema.add_action(kGubsyActionMenuDown, "Menu Down", "Menu");
    schema.add_action(kGubsyActionMenuLeft, "Menu Left", "Menu");
    schema.add_action(kGubsyActionMenuRight, "Menu Right", "Menu");
    schema.add_action(kGubsyActionConfirmJump, "Confirm / Jump", "Shared");
    schema.add_action(kGubsyActionBackAttack, "Back / Attack", "Shared");
    schema.add_action(kGubsyActionPagePreviousBomb, "Page Previous / Bomb", "Shared");
    schema.add_action(kGubsyActionPageNextRope, "Page Next / Rope", "Shared");
    schema.add_action(kGubsyActionMoveUp, "Move Up", "Gameplay");
    schema.add_action(kGubsyActionMoveDown, "Move Down", "Gameplay");
    schema.add_action(kGubsyActionMoveLeft, "Move Left", "Gameplay");
    schema.add_action(kGubsyActionMoveRight, "Move Right", "Gameplay");
    schema.add_action(kGubsyActionUse, "Use", "Gameplay");
    schema.add_axis_1d(0, "Analog Value", "Gameplay");
    schema.add_axis_2d(0, "Analog Move", "Gameplay");
    return schema;
}

} // namespace splonks

#include "input_bind_schema.hpp"

namespace splonks {

BindsSchema BuildGubsyBindsSchema() {
    BindsSchema schema;
    schema.add_action(0, "Menu Up", "Menu");
    schema.add_action(1, "Menu Down", "Menu");
    schema.add_action(2, "Menu Left", "Menu");
    schema.add_action(3, "Menu Right", "Menu");
    schema.add_action(4, "Confirm / Jump", "Shared");
    schema.add_action(5, "Back / Attack", "Shared");
    schema.add_action(6, "Page Previous / Bomb", "Shared");
    schema.add_action(7, "Page Next / Rope", "Shared");
    schema.add_action(8, "Move Up", "Gameplay");
    schema.add_action(9, "Move Down", "Gameplay");
    schema.add_action(10, "Move Left", "Gameplay");
    schema.add_action(11, "Move Right", "Gameplay");
    schema.add_action(12, "Use", "Gameplay");
    schema.add_axis_1d(0, "Analog Value", "Gameplay");
    schema.add_axis_2d(0, "Analog Move", "Gameplay");
    return schema;
}

} // namespace splonks

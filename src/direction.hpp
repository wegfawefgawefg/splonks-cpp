#pragma once

namespace splonks {

enum class Direction {
    Left,
    Right,
    Down,
    Up,
};

Direction Side();
Direction DownOrUp();
Direction SideOrDown();
Direction RandomDirection();

} // namespace splonks

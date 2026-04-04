#pragma once

namespace splonks {

enum class Direction {
    Left,
    Right,
    Down,
    Up,
};

Direction LeftOrRight();
Direction DownOrUp();
Direction LeftOrRightOrDown();
Direction RandomDirection();

} // namespace splonks

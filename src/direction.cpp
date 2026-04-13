#include "direction.hpp"
#include "utils.hpp"

#include <stdexcept>

namespace splonks {

namespace {

} // namespace

Direction LeftOrRight() {
    switch (rng::RandomIntInclusive(0, 1)) {
    case 0:
        return Direction::Left;
    case 1:
        return Direction::Right;
    default:
        throw std::runtime_error("LeftOrRight generated unreachable direction");
    }
}

Direction DownOrUp() {
    switch (rng::RandomIntInclusive(0, 1)) {
    case 0:
        return Direction::Down;
    case 1:
        return Direction::Up;
    default:
        throw std::runtime_error("DownOrUp generated unreachable direction");
    }
}

Direction LeftOrRightOrDown() {
    switch (rng::RandomIntInclusive(0, 2)) {
    case 0:
        return Direction::Left;
    case 1:
        return Direction::Right;
    case 2:
        return Direction::Down;
    default:
        throw std::runtime_error("LeftOrRightOrDown generated unreachable direction");
    }
}

Direction RandomDirection() {
    switch (rng::RandomIntInclusive(0, 3)) {
    case 0:
        return Direction::Left;
    case 1:
        return Direction::Right;
    case 2:
        return Direction::Down;
    case 3:
        return Direction::Up;
    default:
        throw std::runtime_error("RandomDirection generated unreachable direction");
    }
}

} // namespace splonks

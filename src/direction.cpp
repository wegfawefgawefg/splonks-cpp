#include "direction.hpp"

#include <random>
#include <stdexcept>

namespace splonks {

namespace {

int RandomIntInclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(generator);
}

} // namespace

Direction LeftOrRight() {
    switch (RandomIntInclusive(0, 1)) {
    case 0:
        return Direction::Left;
    case 1:
        return Direction::Right;
    default:
        throw std::runtime_error("LeftOrRight generated unreachable direction");
    }
}

Direction DownOrUp() {
    switch (RandomIntInclusive(0, 1)) {
    case 0:
        return Direction::Down;
    case 1:
        return Direction::Up;
    default:
        throw std::runtime_error("DownOrUp generated unreachable direction");
    }
}

Direction LeftOrRightOrDown() {
    switch (RandomIntInclusive(0, 2)) {
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
    switch (RandomIntInclusive(0, 3)) {
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

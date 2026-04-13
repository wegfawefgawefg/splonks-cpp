#include "tile.hpp"
#include "utils.hpp"

#include <stdexcept>

namespace splonks {

namespace {

} // namespace

Tile RandomTile() {
    switch (rng::RandomIntInclusive(0, 5)) {
    case 0:
        return Tile::Air;
    case 1:
        return Tile::Dirt;
    case 2:
        return Tile::Gold;
    case 3:
        return Tile::Block;
    case 4:
        return Tile::Entrance;
    case 5:
        return Tile::Exit;
    case 6:
        return Tile::LadderTop;
    case 7:
        return Tile::Ladder;
    case 8:
        return Tile::Spikes;
    default:
        throw std::runtime_error("RandomTile generated unreachable tile");
    }
}

bool IsTileCollidable(Tile tile) {
    switch (tile) {
    case Tile::Dirt:
        return true;
    case Tile::Block:
        return true;
    case Tile::Gold:
        return true;
    case Tile::GoldBig:
        return true;
    case Tile::ShopWall:
        return true;
    case Tile::SmoothWall:
        return true;
    case Tile::Glass:
        return true;
    default:
        return false;
    }
}

bool CollidableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        const bool collided = IsTileCollidable(*tile);
        if (collided) {
            return true;
        }
    }
    return false;
}

bool ClimbableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        bool climbable = false;
        switch (*tile) {
        case Tile::Ladder:
            climbable = true;
            break;
        case Tile::LadderTop:
            climbable = true;
            break;
        case Tile::Rope:
            climbable = true;
            break;
        default:
            break;
        }

        if (climbable) {
            return true;
        }
    }
    return false;
}

} // namespace splonks

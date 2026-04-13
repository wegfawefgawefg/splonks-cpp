#include "tile.hpp"
#include "tile_archetype.hpp"
#include "utils.hpp"


namespace splonks {

Tile RandomTile() {
    const int tile_index = rng::RandomIntInclusive(0, static_cast<int>(kTileCount) - 1);
    return static_cast<Tile>(tile_index);
}

bool CollidableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        if (IsTileCollidable(*tile)) {
            return true;
        }
    }
    return false;
}

bool ClimbableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        if (GetTileArchetype(*tile).climbable) {
            return true;
        }
    }
    return false;
}

bool HangableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        if (GetTileArchetype(*tile).hangable) {
            return true;
        }
    }
    return false;
}

} // namespace splonks

#include "tile.hpp"

#include "stage.hpp"
#include "tile_spec.hpp"
#include "utils.hpp"

namespace splonks {

Tile RandomTile() {
    const int tile_index = rng::RandomIntInclusive(0, static_cast<int>(kTileCount) - 1);
    return static_cast<Tile>(tile_index);
}

const char* TileToString(Tile tile) {
    return GetTileSpec(tile).debug_name;
}

TileRotation NormalizeTileRotation(int rotation) {
    int normalized = rotation % 4;
    if (normalized < 0) {
        normalized += 4;
    }
    return static_cast<TileRotation>(normalized) & kTileRotationMask;
}

TileRotation RotateTileRotation(TileRotation rotation, int quarter_turns) {
    return NormalizeTileRotation(static_cast<int>(rotation & kTileRotationMask) + quarter_turns);
}

bool IsTileClimbableWithRotation(Tile tile, TileRotation rotation) {
    const TileSpec& spec = GetTileSpec(tile);
    if (!spec.climbable) {
        return false;
    }
    const std::uint8_t rotation_bit =
        static_cast<std::uint8_t>(1U << NormalizeTileRotation(rotation));
    return (spec.climbable_rotation_mask & rotation_bit) != 0;
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
        if (GetTileSpec(*tile).climbable) {
            return true;
        }
    }
    return false;
}

bool HangableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        if (GetTileSpec(*tile).hangable) {
            return true;
        }
    }
    return false;
}

} // namespace splonks

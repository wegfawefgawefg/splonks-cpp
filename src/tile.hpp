#pragma once

#include <vector>

namespace splonks {

enum class Tile {
    Air,
    Dirt,
    Gold,
    Block,
    LadderTop,
    Ladder,
    Spikes,
    Rope,
    Entrance,
    Exit,
};

constexpr unsigned int kTileSize = 16;
constexpr std::size_t kTileCount = 10;

Tile RandomTile();
bool IsTileCollidable(Tile tile);
bool CollidableTileInList(const std::vector<const Tile*>& tiles);
bool ClimbableTileInList(const std::vector<const Tile*>& tiles);

} // namespace splonks

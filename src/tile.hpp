#pragma once

#include <cstddef>
#include <vector>

namespace splonks {

enum class Tile {
    Air,
    Dirt,
    Gold,
    GoldBig,
    Block,
    ShopWall,
    SmoothWall,
    Glass,
    LadderTop,
    Ladder,
    Spikes,
    Rope,
    Entrance,
    Exit,
};

constexpr unsigned int kTileSize = 16;
constexpr std::size_t kTileCount = 14;

Tile RandomTile();
bool IsTileTransparent(Tile tile);
bool IsTileCollidable(Tile tile);
bool IsTileHangable(Tile tile);
bool CollidableTileInList(const std::vector<const Tile*>& tiles);
bool ClimbableTileInList(const std::vector<const Tile*>& tiles);
bool HangableTileInList(const std::vector<const Tile*>& tiles);

} // namespace splonks

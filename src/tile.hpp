#pragma once

#include <cstddef>
#include <vector>

namespace splonks {

enum class StageType : int;

enum class Tile {
    Air,
    CaveAir0,
    CaveAir1,
    CaveAir2,
    CaveDirt,
    CaveBlock,
    CaveShopWall,
    CaveSmoothWall,
    Glass,
    LawsonWall,
    LawsonInside,
    LawsonLeftTopper,
    LawsonMiddleTopper,
    LawsonRightTopper,
    LawsonFloor,
    IceAir0,
    IceAir1,
    IceAir2,
    IceDirt,
    IceBlock,
    JungleAir0,
    JungleAir1,
    JungleAir2,
    JungleDirt,
    JungleBlock,
    TempleAir0,
    TempleAir1,
    TempleAir2,
    TempleDirt,
    TempleGold,
    TempleBlock,
    BossAir0,
    BossAir1,
    BossAir2,
    BossDirt,
    BossBlock,
    LadderTop,
    Ladder,
    LadderOrange,
    Spikes,
    Rope,
    Vine,
    VineTop,
    WaterSwim,
    Lava,
    Lush,
    Tree,
    ThinIce,
    Dark,
    DarkFall,
    AlienShip,
    TempleFake,
    Entrance,
    Exit,
};

constexpr unsigned int kTileSize = 16;
constexpr std::size_t kTileCount = 54;

Tile RandomTile();
const char* TileToString(Tile tile);
bool IsTileTransparent(Tile tile);
bool IsTileCollidable(Tile tile);
bool IsTileHangable(Tile tile);
bool CollidableTileInList(const std::vector<const Tile*>& tiles);
bool ClimbableTileInList(const std::vector<const Tile*>& tiles);
bool HangableTileInList(const std::vector<const Tile*>& tiles);

} // namespace splonks

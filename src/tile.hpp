#pragma once

#include <cstddef>
#include <cstdint>
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
using TileRotation = std::uint8_t;
constexpr TileRotation kTileRotationMask = 0x03;
constexpr TileRotation kTileRotation0 = 0;
constexpr TileRotation kTileRotation90 = 1;
constexpr TileRotation kTileRotation180 = 2;
constexpr TileRotation kTileRotation270 = 3;
constexpr std::uint8_t kTileRotationBit0 = 1 << kTileRotation0;
constexpr std::uint8_t kTileRotationBit90 = 1 << kTileRotation90;
constexpr std::uint8_t kTileRotationBit180 = 1 << kTileRotation180;
constexpr std::uint8_t kTileRotationBit270 = 1 << kTileRotation270;
constexpr std::uint8_t kTileRotationBitAll =
    kTileRotationBit0 | kTileRotationBit90 | kTileRotationBit180 | kTileRotationBit270;

Tile RandomTile();
const char* TileToString(Tile tile);
TileRotation NormalizeTileRotation(int rotation);
TileRotation RotateTileRotation(TileRotation rotation, int quarter_turns);
bool IsTileTransparent(Tile tile);
bool IsTileCollidable(Tile tile);
bool IsTileOneWayTopSolid(Tile tile);
bool IsTileGroundSupport(Tile tile);
bool IsTileHangable(Tile tile);
bool IsTileClimbableWithRotation(Tile tile, TileRotation rotation);
bool CollidableTileInList(const std::vector<const Tile*>& tiles);
bool ClimbableTileInList(const std::vector<const Tile*>& tiles);
bool HangableTileInList(const std::vector<const Tile*>& tiles);

} // namespace splonks

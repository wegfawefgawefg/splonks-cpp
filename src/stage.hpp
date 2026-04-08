#pragma once

#include "math_types.hpp"
#include "tile.hpp"
#include "utils.hpp"

#include <optional>
#include <vector>

namespace splonks {

enum class StageType : int {
    Blank,
    Test1,
    Cave1,
    Cave2,
    Cave3,
    Ice1,
    Ice2,
    Ice3,
    Desert1,
    Desert2,
    Desert3,
    Temple1,
    Temple2,
    Temple3,
    Boss,
};

struct Stage {
    StageType stage_type = StageType::Blank;
    std::vector<std::vector<Tile>> tiles;
    std::vector<std::vector<int>> rooms;
    std::vector<IVec2> path;
    float gravity = 0.3F;

    static const UVec2 kShape;
    static const UVec2 kRoomShape;
    static const UVec2 kRoomLayout;

    static Stage NewBlank();
    static Stage New(StageType stage_type);
    UVec2 GetStageDims() const;
    UVec2 GetRoomLayoutDims() const;
    UVec2 GetRoomDims() const;
    IVec2 GetRoomTlWc(const IVec2& room) const;
    const Tile& GetTile(unsigned int x, unsigned int y) const;
    const Tile* GetTileAtWc(const IVec2& pos) const;
    std::vector<const Tile*> GetTilesInRectWc(const IVec2& tl, const IVec2& br) const;
    std::vector<const Tile*> GetTilesInRect(const IVec2& tl, const IVec2& br) const;
    void SetTile(const IVec2& pos, Tile tile);
    void SetTilesInRectWc(const AABB& area, Tile tile_type);
    void SetTilesInRect(const AABB& area, Tile tile_type);
    std::vector<IAABB> GetAabbsForAllCollidableTilesInRect(const IVec2& tl, const IVec2& br) const;
    UVec2 GetRandomRoom() const;
    std::optional<IVec2> GetRandomNoncollidablePositionInRandomRoom() const;
    std::optional<IVec2> GetRandomNoncollidablePositionInRoom(const UVec2& room) const;
    unsigned int GetWidth() const;
    unsigned int GetHeight() const;
    unsigned int GetTileWidth() const;
    unsigned int GetTileHeight() const;
    std::pair<UVec2, UVec2> GetRoomCorners(const UVec2& room) const;
    std::vector<const Tile*> GetTilesInRoom(const UVec2& room) const;
    IVec2 GetStartingRoom() const;
    IVec2 GetTileCoordAtWc(const IVec2& wc) const;
    bool TileCoordAtWcExists(const IVec2& wc) const;
};

} // namespace splonks

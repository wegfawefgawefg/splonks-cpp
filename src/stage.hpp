#pragma once

#include "entity/core_types.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "tile.hpp"
#include "utils.hpp"

#include <optional>
#include <string>
#include <vector>

namespace splonks {

struct StageEntitySpawn {
    EntityType type_ = EntityType::None;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    LeftOrRight facing = LeftOrRight::Left;
    FrameDataId animation_id = kInvalidFrameDataId;
};

enum class BackgroundStampCondition {
    None,
    Wanted,
};

struct BackgroundStamp {
    FrameDataId animation_id = kInvalidFrameDataId;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    BackgroundStampCondition condition = BackgroundStampCondition::None;
};

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
    std::vector<std::vector<EntityType>> embedded_treasures;
    std::vector<std::vector<int>> rooms;
    std::vector<IVec2> path;
    std::vector<StageEntitySpawn> entity_spawns;
    std::vector<BackgroundStamp> background_stamps;
    float gravity = 0.3F;
    Tile stage_border_tile = Tile::Dirt;
    Vec2 camera_clamp_margin = Vec2::New(0.0F, 0.0F);

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
    EntityType GetEmbeddedTreasure(unsigned int x, unsigned int y) const;
    const Tile* GetTileAtWc(const IVec2& pos) const;
    std::vector<const Tile*> GetTilesInRectWc(const IVec2& tl, const IVec2& br) const;
    std::vector<const Tile*> GetTilesInRect(const IVec2& tl, const IVec2& br) const;
    void SetTile(const IVec2& pos, Tile tile);
    void SetEmbeddedTreasure(const IVec2& pos, EntityType type_);
    EntityType TakeEmbeddedTreasure(const IVec2& pos);
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

#pragma once

#include "entity/core_types.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "tile.hpp"
#include "utils.hpp"

#include <optional>
#include <string>
#include <cstdint>
#include <vector>

namespace splonks {

struct Entity;

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

struct StageLight {
    VID vid;
    IVec2 tile_pos = IVec2::New(0, 0);
    int radius = 0;
};

enum class StageType : int {
    Blank,
    Test1,
    SplkMines1,
    SplkMines2,
    SplkMines3,
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

enum class StageBorderSideKind : std::uint8_t {
    Left,
    Right,
    Top,
    Bottom,
};

struct StageBorderSide {
    Tile tile = Tile::Air;
};

struct StageBorder {
    StageBorderSide left;
    StageBorderSide right;
    StageBorderSide top;
    StageBorderSide bottom;
    bool wrap_x = false;
    bool wrap_y = false;
    std::optional<int> void_death_y = std::nullopt;
};

struct Stage {
    StageType stage_type = StageType::Blank;
    std::vector<std::vector<Tile>> tiles;
    std::vector<std::vector<Tile>> backwall_tiles;
    std::vector<std::vector<EntityType>> embedded_treasures;
    std::vector<std::vector<int>> rooms;
    std::vector<IVec2> path;
    std::vector<StageEntitySpawn> entity_spawns;
    std::vector<BackgroundStamp> background_stamps;
    std::vector<StageLight> lights;
    float gravity = 0.3F;
    StageBorder border{};
    bool camera_clamp_enabled = true;
    Vec2 camera_clamp_margin = Vec2::New(0.0F, 0.0F);
    bool wrap_transform_active = false;
    unsigned int wrap_padding_chunks = 0;
    UVec2 wrap_core_origin_tiles = UVec2::New(0, 0);
    UVec2 wrap_core_size_tiles = UVec2::New(0, 0);
    std::uint32_t next_light_vid = 0;

    static const UVec2 kShape;
    static const UVec2 kRoomShape;
    static const UVec2 kRoomLayout;

    static Stage NewBlank();
    static Stage New(StageType stage_type);
    static StageBorder MakeUniformBorder(Tile tile);
    UVec2 GetStageDims() const;
    UVec2 GetRoomLayoutDims() const;
    UVec2 GetRoomDims() const;
    IVec2 GetRoomTlWc(const IVec2& room) const;
    const Tile& GetTile(unsigned int x, unsigned int y) const;
    const Tile& GetBackwallTile(unsigned int x, unsigned int y) const;
    EntityType GetEmbeddedTreasure(unsigned int x, unsigned int y) const;
    const Tile* GetTileAtWc(const IVec2& pos) const;
    std::vector<const Tile*> GetTilesInRectWc(const IVec2& tl, const IVec2& br) const;
    std::vector<const Tile*> GetTilesInRect(const IVec2& tl, const IVec2& br) const;
    void FillBackwall(const std::vector<Tile>& fill_tiles);
    void SetTile(const IVec2& pos, Tile tile);
    void SetBackwallTile(const IVec2& pos, Tile tile);
    void SetEmbeddedTreasure(const IVec2& pos, EntityType type_);
    EntityType TakeEmbeddedTreasure(const IVec2& pos);
    VID AddLight(const IVec2& tile_pos, int radius);
    bool RemoveLight(VID vid);
    const StageLight* GetLight(VID vid) const;
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
    bool WrapsX() const;
    bool WrapsY() const;
    bool HasVoidDeathY() const;
    float GetVoidDeathY() const;
    const StageBorderSide& GetBorderSide(StageBorderSideKind side) const;
    Tile GetBorderTile(StageBorderSideKind side) const;
    bool IsBorderSideBlocking(StageBorderSideKind side) const;
    std::optional<StageBorderSideKind> GetOutOfBoundsSideForTileCoord(int tile_x, int tile_y) const;
    std::optional<StageBorderSideKind> GetOutOfBoundsSideForWorldPos(const IVec2& wc) const;
    Tile GetTileOrBorder(int tile_x, int tile_y) const;
    bool IsTileCoordInside(int tile_x, int tile_y) const;
    bool IsWorldPosInside(const IVec2& wc) const;
    IVec2 WrapTileCoord(const IVec2& tile_coord) const;
    IVec2 WrapWorldPos(const IVec2& wc) const;
    void NormalizeEntityPositionForWrap(Entity& entity) const;
    std::pair<UVec2, UVec2> GetRoomCorners(const UVec2& room) const;
    std::vector<const Tile*> GetTilesInRoom(const UVec2& room) const;
    IVec2 GetStartingRoom() const;
    IVec2 GetTileCoordAtWc(const IVec2& wc) const;
    bool TileCoordAtWcExists(const IVec2& wc) const;
};

} // namespace splonks

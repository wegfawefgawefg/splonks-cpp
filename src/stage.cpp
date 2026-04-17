#include "stage.hpp"

#include "entity.hpp"
#include "room.hpp"
#include "stage_gen/splk_mines.hpp"

#include <algorithm>
#include <cmath>

namespace splonks {

namespace {

bool RandomBool() {
    return rng::RandomIntExclusive(0, 2) == 0;
}

int WrapCoordinate(int value, int size) {
    if (size <= 0) {
        return value;
    }

    int wrapped = value % size;
    if (wrapped < 0) {
        wrapped += size;
    }
    return wrapped;
}

int FloorDiv(int value, int divisor) {
    if (divisor == 0) {
        return 0;
    }

    int result = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        result -= 1;
    }
    return result;
}

unsigned int GetTileRowWidth(const std::vector<std::vector<Tile>>& tiles) {
    if (tiles.empty()) {
        return 0;
    }
    return static_cast<unsigned int>(tiles.front().size());
}

std::vector<std::vector<EntityType>> MakeEmptyEmbeddedTreasures(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<EntityType>> embedded_treasures;
    embedded_treasures.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        embedded_treasures.push_back(
            std::vector<EntityType>(row.size(), EntityType::None)
        );
    }
    return embedded_treasures;
}

std::vector<std::vector<Tile>> MakeEmptyBackwallTiles(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<Tile>> backwall_tiles;
    backwall_tiles.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        backwall_tiles.push_back(std::vector<Tile>(row.size(), Tile::Air));
    }
    return backwall_tiles;
}

bool EmbeddedTreasureCoordExists(
    const std::vector<std::vector<EntityType>>& embedded_treasures,
    int tile_x,
    int tile_y
) {
    if (tile_x < 0 || tile_y < 0) {
        return false;
    }
    if (tile_y >= static_cast<int>(embedded_treasures.size())) {
        return false;
    }
    const std::vector<EntityType>& row =
        embedded_treasures[static_cast<std::size_t>(tile_y)];
    return tile_x < static_cast<int>(row.size());
}

} // namespace

const UVec2 Stage::kShape = UVec2::New(40, 32);
const UVec2 Stage::kRoomShape = UVec2::New(10, 8);
const UVec2 Stage::kRoomLayout = UVec2::New(4, 4);

Stage Stage::NewBlank() {
    Stage stage;
    stage.stage_type = StageType::Blank;
    stage.tiles = std::vector<std::vector<Tile>>(1, std::vector<Tile>(1, Tile::Air));
    stage.backwall_tiles = MakeEmptyBackwallTiles(stage.tiles);
    stage.embedded_treasures = MakeEmptyEmbeddedTreasures(stage.tiles);
    stage.rooms = {};
    stage.path = {};
    stage.lights = {};
    stage.gravity = 0.3F;
    stage.border = MakeUniformBorder(Tile::Air);
    stage.camera_clamp_margin = Vec2::New(0.0F, 0.0F);
    stage.camera_clamp_enabled = true;
    return stage;
}

Stage Stage::New(StageType stage_type) {
    if (stage_gen::splk_mines::UsesSplkMinesGenerator(stage_type)) {
        return stage_gen::splk_mines::GenerateStage(stage_type);
    }

    std::vector<std::vector<int>> rooms(
        static_cast<std::size_t>(kRoomLayout.y),
        std::vector<int>(static_cast<std::size_t>(kRoomLayout.x),
                         static_cast<int>(RoomType::Box)));
    std::vector<IVec2> path;

    {
        IVec2 current_room_pos = IVec2::New(
            rng::RandomIntExclusive(0, static_cast<int>(kRoomLayout.x)), 0);

        for (unsigned int floor = 0; floor < kRoomLayout.y; ++floor) {
            const int go_down_x =
                rng::RandomIntExclusive(0, static_cast<int>(kRoomLayout.x));
            const int direction = (current_room_pos.x - go_down_x) > 0
                                      ? 1
                                      : ((current_room_pos.x - go_down_x) < 0 ? -1 : 0);
            while (current_room_pos.x != go_down_x) {
                path.push_back(current_room_pos);
                current_room_pos.x -= direction;
            }
            path.push_back(current_room_pos);
            current_room_pos.y += 1;
        }

        const IVec2 first_room = path.front();
        const IVec2 second_room = path[1];
        const RoomType first_room_type =
            second_room.x > first_room.x || second_room.x < first_room.x ? RoomType::LeftRight
                                                                         : RoomType::LeftDownRight;
        rooms[static_cast<std::size_t>(first_room.y)][static_cast<std::size_t>(first_room.x)] =
            static_cast<int>(first_room_type);

        for (std::size_t path_num = 1; path_num < path.size() - 1; ++path_num) {
            const IVec2 prev_room = path[path_num - 1];
            const IVec2 room = path[path_num];
            const IVec2 next_room = path[path_num + 1];
            RoomType room_type = RoomType::LeftRight;

            if (prev_room.y < room.y && room.y < next_room.y) {
                room_type = RoomType::FourWay;
            } else if (prev_room.y < room.y && room.y == next_room.y) {
                room_type = RoomType::LeftUpRight;
            } else if (prev_room.y == room.y && next_room.y > room.y) {
                room_type = RoomType::LeftDownRight;
            } else if (prev_room.y == room.y && next_room.y == room.y) {
                room_type = RoomType::LeftRight;
            }

            rooms[static_cast<std::size_t>(room.y)][static_cast<std::size_t>(room.x)] =
                static_cast<int>(room_type);
        }

        const IVec2 last_room = path.back();
        rooms[static_cast<std::size_t>(last_room.y)][static_cast<std::size_t>(last_room.x)] =
            static_cast<int>(RoomType::Exit);

        rooms[static_cast<std::size_t>(first_room.y)][static_cast<std::size_t>(first_room.x)] =
            static_cast<int>(RoomType::Entrance);
    }

    std::vector<std::vector<Tile>> tiles(
        static_cast<std::size_t>(kShape.y),
        std::vector<Tile>(static_cast<std::size_t>(kShape.x), Tile::Air));
    Tile border_tile = Tile::CaveDirt;
    std::vector<Tile> backwall_fill_tiles{
        Tile::CaveAir0,
        Tile::CaveAir1,
        Tile::CaveAir2,
    };
    switch (stage_type) {
    case StageType::Ice1:
    case StageType::Ice2:
    case StageType::Ice3:
        border_tile = Tile::IceDirt;
        backwall_fill_tiles = {Tile::IceAir0, Tile::IceAir1, Tile::IceAir2};
        break;
    case StageType::Desert1:
    case StageType::Desert2:
    case StageType::Desert3:
        border_tile = Tile::JungleDirt;
        backwall_fill_tiles = {Tile::JungleAir0, Tile::JungleAir1, Tile::JungleAir2};
        break;
    case StageType::Temple1:
    case StageType::Temple2:
    case StageType::Temple3:
        border_tile = Tile::TempleDirt;
        backwall_fill_tiles = {Tile::TempleAir0, Tile::TempleAir1, Tile::TempleAir2};
        break;
    case StageType::Boss:
        border_tile = Tile::BossDirt;
        backwall_fill_tiles = {Tile::BossAir0, Tile::BossAir1, Tile::BossAir2};
        break;
    case StageType::Blank:
    case StageType::Test1:
    case StageType::SplkMines1:
    case StageType::SplkMines2:
    case StageType::SplkMines3:
        border_tile = Tile::CaveDirt;
        break;
    }

    for (unsigned int room_y = 0; room_y < kRoomLayout.y; ++room_y) {
        for (unsigned int room_x = 0; room_x < kRoomLayout.x; ++room_x) {
            const UVec2 room_pos = UVec2::New(room_x, room_y) * kRoomShape;
            const RoomType room_type =
                static_cast<RoomType>(rooms[static_cast<std::size_t>(room_y)]
                                           [static_cast<std::size_t>(room_x)]);
            const std::vector<std::vector<Tile>> room = GenRoom(room_type, stage_type, border_tile);

            const bool flip = RandomBool();
            for (unsigned int tile_y = 0; tile_y < kRoomShape.y; ++tile_y) {
                for (unsigned int tile_x = 0; tile_x < kRoomShape.x; ++tile_x) {
                    const UVec2 tile_pos = room_pos + UVec2::New(tile_x, tile_y);
                    unsigned int tile_sample_x = tile_x;
                    if (flip) {
                        tile_sample_x = kRoomShape.x - tile_x - 1;
                    }

                    tiles[static_cast<std::size_t>(tile_pos.y)]
                         [static_cast<std::size_t>(tile_pos.x)] =
                             room[static_cast<std::size_t>(tile_y)]
                                 [static_cast<std::size_t>(tile_sample_x)];
                }
            }
        }
    }

    Stage stage;
    stage.stage_type = stage_type;
    stage.tiles = std::move(tiles);
    stage.backwall_tiles = MakeEmptyBackwallTiles(stage.tiles);
    stage.FillBackwall(backwall_fill_tiles);
    stage.embedded_treasures = MakeEmptyEmbeddedTreasures(stage.tiles);
    stage.rooms = std::move(rooms);
    stage.path = std::move(path);
    stage.lights = {};
    stage.gravity = 0.3F;
    stage.border = MakeUniformBorder(border_tile);
    stage.camera_clamp_margin = ToVec2(kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;
    return stage;
}

StageBorder Stage::MakeUniformBorder(Tile tile) {
    StageBorder border;
    border.left.tile = tile;
    border.right.tile = tile;
    border.top.tile = tile;
    border.bottom.tile = tile;
    return border;
}

UVec2 Stage::GetStageDims() const {
    return UVec2::New(GetTileWidth(), GetTileHeight()) * kTileSize;
}

UVec2 Stage::GetRoomLayoutDims() const {
    if (rooms.empty() || rooms.front().empty()) {
        return UVec2::New(1, 1);
    }
    return UVec2::New(
        static_cast<unsigned int>(rooms.front().size()),
        static_cast<unsigned int>(rooms.size())
    );
}

UVec2 Stage::GetRoomDims() const {
    const UVec2 room_layout_dims = GetRoomLayoutDims();
    if (room_layout_dims.x == 0 || room_layout_dims.y == 0) {
        return GetStageDims();
    }
    return UVec2::New(
        GetTileWidth() / room_layout_dims.x,
        GetTileHeight() / room_layout_dims.y
    ) * kTileSize;
}

IVec2 Stage::GetRoomTlWc(const IVec2& room) const {
    const UVec2 room_dims = GetRoomDims();
    return IVec2::New(
        room.x * static_cast<int>(room_dims.x),
        room.y * static_cast<int>(room_dims.y)
    );
}

const Tile& Stage::GetTile(unsigned int x, unsigned int y) const {
    return tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

const Tile& Stage::GetBackwallTile(unsigned int x, unsigned int y) const {
    return backwall_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

EntityType Stage::GetEmbeddedTreasure(unsigned int x, unsigned int y) const {
    if (!EmbeddedTreasureCoordExists(
            embedded_treasures,
            static_cast<int>(x),
            static_cast<int>(y))) {
        return EntityType::None;
    }
    return embedded_treasures[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

const Tile* Stage::GetTileAtWc(const IVec2& pos) const {
    if (!TileCoordAtWcExists(pos)) {
        return nullptr;
    }

    const IVec2 tile_coords = GetTileCoordAtWc(pos);
    if (!IsTileCoordInside(tile_coords.x, tile_coords.y)) {
        return nullptr;
    }

    return &GetTile(static_cast<unsigned int>(tile_coords.x), static_cast<unsigned int>(tile_coords.y));
}

std::vector<const Tile*> Stage::GetTilesInRectWc(const IVec2& tl, const IVec2& br) const {
    return GetTilesInRect(
        IVec2::New(
            FloorDiv(tl.x, static_cast<int>(kTileSize)),
            FloorDiv(tl.y, static_cast<int>(kTileSize))
        ),
        IVec2::New(
            FloorDiv(br.x, static_cast<int>(kTileSize)),
            FloorDiv(br.y, static_cast<int>(kTileSize))
        )
    );
}

std::vector<const Tile*> Stage::GetTilesInRect(const IVec2& tl, const IVec2& br) const {
    std::vector<const Tile*> result;
    for (int y = tl.y; y <= br.y; ++y) {
        for (int x = tl.x; x <= br.x; ++x) {
            const IVec2 tile_pos = WrapTileCoord(IVec2::New(x, y));
            if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                continue;
            }
            result.push_back(&GetTile(static_cast<unsigned int>(tile_pos.x), static_cast<unsigned int>(tile_pos.y)));
        }
    }
    return result;
}

void Stage::FillBackwall(const std::vector<Tile>& fill_tiles) {
    backwall_tiles = MakeEmptyBackwallTiles(tiles);
    if (fill_tiles.empty()) {
        return;
    }

    for (std::size_t y = 0; y < backwall_tiles.size(); ++y) {
        for (std::size_t x = 0; x < backwall_tiles[y].size(); ++x) {
            const int fill_index =
                rng::RandomIntInclusive(0, static_cast<int>(fill_tiles.size()) - 1);
            backwall_tiles[y][x] = fill_tiles[static_cast<std::size_t>(fill_index)];
        }
    }
}

void Stage::SetTile(const IVec2& pos, Tile tile) {
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] = tile;
}

void Stage::SetBackwallTile(const IVec2& pos, Tile tile) {
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    backwall_tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] = tile;
}

void Stage::SetEmbeddedTreasure(const IVec2& pos, EntityType type_) {
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    if (!EmbeddedTreasureCoordExists(embedded_treasures, tile_pos.x, tile_pos.y)) {
        return;
    }
    embedded_treasures[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] = type_;
}

EntityType Stage::TakeEmbeddedTreasure(const IVec2& pos) {
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return EntityType::None;
    }
    if (!EmbeddedTreasureCoordExists(embedded_treasures, tile_pos.x, tile_pos.y)) {
        return EntityType::None;
    }

    EntityType& treasure =
        embedded_treasures[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)];
    const EntityType result = treasure;
    treasure = EntityType::None;
    return result;
}

VID Stage::AddLight(const IVec2& tile_pos, int radius) {
    const VID vid = VID{next_light_vid++};
    lights.push_back(StageLight{
        .vid = vid,
        .tile_pos = tile_pos,
        .radius = radius,
    });
    return vid;
}

bool Stage::RemoveLight(VID vid) {
    const auto it = std::remove_if(
        lights.begin(),
        lights.end(),
        [vid](const StageLight& light) { return light.vid == vid; }
    );
    if (it == lights.end()) {
        return false;
    }
    lights.erase(it, lights.end());
    return true;
}

const StageLight* Stage::GetLight(VID vid) const {
    for (const StageLight& light : lights) {
        if (light.vid == vid) {
            return &light;
        }
    }
    return nullptr;
}

void Stage::SetTilesInRectWc(const AABB& area, Tile tile_type) {
    AABB area_tc;
    area_tc.tl = area.tl / static_cast<float>(kTileSize);
    area_tc.br = area.br / static_cast<float>(kTileSize);
    SetTilesInRect(area_tc, tile_type);
}

void Stage::SetTilesInRect(const AABB& area, Tile tile_type) {
    const int max_x = static_cast<int>(GetTileWidth()) - 1;
    const int max_y = static_cast<int>(GetTileHeight()) - 1;
    const IVec2 tl = IVec2::New(static_cast<int>(area.tl.x), static_cast<int>(area.tl.y));
    const IVec2 br = IVec2::New(static_cast<int>(area.br.x), static_cast<int>(area.br.y));

    for (int y = (tl.y < 0 ? 0 : tl.y); y <= (br.y < max_y ? br.y : max_y); ++y) {
        for (int x = (tl.x < 0 ? 0 : tl.x); x <= (br.x < max_x ? br.x : max_x); ++x) {
            if (tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == Tile::Exit) {
                continue;
            }
            tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile_type;
        }
    }
}

std::vector<IAABB> Stage::GetAabbsForAllCollidableTilesInRect(const IVec2& tl,
                                                              const IVec2& br) const {
    const UVec2 stage_dims_wc = GetStageDims();
    if (tl.x >= static_cast<int>(stage_dims_wc.x) || tl.y >= static_cast<int>(stage_dims_wc.y)) {
        return {};
    }

    const int max_x = static_cast<int>(stage_dims_wc.x) - 1;
    const int max_y = static_cast<int>(stage_dims_wc.y) - 1;
    const IVec2 clamped_tl = IVec2::New(tl.x < 0 ? 0 : tl.x, tl.y < 0 ? 0 : tl.y);
    const IVec2 clamped_br =
        IVec2::New(br.x < max_x ? br.x : max_x, br.y < max_y ? br.y : max_y);
    const IVec2 tl_tc = clamped_tl / static_cast<int>(kTileSize);
    const IVec2 br_tc = clamped_br / static_cast<int>(kTileSize);

    std::vector<IAABB> result;
    for (int y = tl_tc.y; y <= br_tc.y; ++y) {
        for (int x = tl_tc.x; x <= br_tc.x; ++x) {
            const Tile tile = GetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y));
            if (IsTileCollidable(tile)) {
                const IVec2 tile_pos = IVec2::New(x, y) * static_cast<int>(kTileSize);
                IAABB aabb;
                aabb.tl = tile_pos;
                aabb.br = tile_pos + IVec2::New(static_cast<int>(kTileSize), static_cast<int>(kTileSize)) -
                          IVec2::New(1, 1);
                result.push_back(aabb);
            }
        }
    }
    return result;
}

UVec2 Stage::GetRandomRoom() const {
    const UVec2 room_layout_dims = GetRoomLayoutDims();
    return UVec2::New(
        static_cast<unsigned int>(rng::RandomIntExclusive(0, static_cast<int>(room_layout_dims.x))),
        static_cast<unsigned int>(rng::RandomIntExclusive(0, static_cast<int>(room_layout_dims.y)))
    );
}

std::optional<IVec2> Stage::GetRandomNoncollidablePositionInRandomRoom() const {
    const UVec2 random_room = GetRandomRoom();
    return GetRandomNoncollidablePositionInRoom(random_room);
}

std::optional<IVec2> Stage::GetRandomNoncollidablePositionInRoom(const UVec2& room) const {
    const auto [room_tl, room_br] = GetRoomCorners(room);

    if (static_cast<int>(room_tl.x) > static_cast<int>(GetTileWidth()) ||
        static_cast<int>(room_tl.y) > static_cast<int>(GetTileHeight())) {
        return std::nullopt;
    }

    const IVec2 tl = IVec2::New(
        std::clamp(static_cast<int>(room_tl.x), 0, static_cast<int>(GetTileWidth()) - 1),
        std::clamp(static_cast<int>(room_tl.y), 0, static_cast<int>(GetTileHeight()) - 1)
    );
    const IVec2 br = IVec2::New(
        std::clamp(static_cast<int>(room_br.x), 0, static_cast<int>(GetTileWidth()) - 1),
        std::clamp(static_cast<int>(room_br.y), 0, static_cast<int>(GetTileHeight()) - 1)
    );

    std::vector<IVec2> noncollidable_tile_coords;
    for (int y = tl.y; y <= br.y; ++y) {
        for (int x = tl.x; x <= br.x; ++x) {
            const Tile tile = GetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y));
            if (!IsTileCollidable(tile)) {
                noncollidable_tile_coords.push_back(IVec2::New(x, y));
            }
        }
    }

    if (noncollidable_tile_coords.empty()) {
        return std::nullopt;
    }

    const int random_tile_idx =
        rng::RandomIntExclusive(0, static_cast<int>(noncollidable_tile_coords.size()));
    const IVec2 tile_coord = noncollidable_tile_coords[static_cast<std::size_t>(random_tile_idx)];
    return tile_coord * static_cast<int>(kTileSize);
}

unsigned int Stage::GetWidth() const {
    return GetTileWidth() * kTileSize;
}

unsigned int Stage::GetHeight() const {
    return GetTileHeight() * kTileSize;
}

unsigned int Stage::GetTileWidth() const {
    return GetTileRowWidth(tiles);
}

unsigned int Stage::GetTileHeight() const {
    return static_cast<unsigned int>(tiles.size());
}

bool Stage::WrapsX() const {
    return border.wrap_x;
}

bool Stage::WrapsY() const {
    return border.wrap_y;
}

bool Stage::HasVoidDeathY() const {
    return border.void_death_y.has_value();
}

float Stage::GetVoidDeathY() const {
    return static_cast<float>(border.void_death_y.value_or(0));
}

const StageBorderSide& Stage::GetBorderSide(StageBorderSideKind side) const {
    switch (side) {
    case StageBorderSideKind::Left:
        return border.left;
    case StageBorderSideKind::Right:
        return border.right;
    case StageBorderSideKind::Top:
        return border.top;
    case StageBorderSideKind::Bottom:
        return border.bottom;
    }

    return border.left;
}

Tile Stage::GetBorderTile(StageBorderSideKind side) const {
    if ((side == StageBorderSideKind::Left || side == StageBorderSideKind::Right) && WrapsX()) {
        return Tile::Air;
    }
    if ((side == StageBorderSideKind::Top || side == StageBorderSideKind::Bottom) && WrapsY()) {
        return Tile::Air;
    }
    return GetBorderSide(side).tile;
}

bool Stage::IsBorderSideBlocking(StageBorderSideKind side) const {
    const Tile tile = GetBorderTile(side);
    return tile != Tile::Air && IsTileCollidable(tile);
}

std::optional<StageBorderSideKind> Stage::GetOutOfBoundsSideForTileCoord(int tile_x, int tile_y) const {
    if (tile_x < 0 && !WrapsX()) {
        return StageBorderSideKind::Left;
    }
    if (tile_x >= static_cast<int>(GetTileWidth()) && !WrapsX()) {
        return StageBorderSideKind::Right;
    }
    if (tile_y < 0 && !WrapsY()) {
        return StageBorderSideKind::Top;
    }
    if (tile_y >= static_cast<int>(GetTileHeight()) && !WrapsY()) {
        return StageBorderSideKind::Bottom;
    }
    return std::nullopt;
}

std::optional<StageBorderSideKind> Stage::GetOutOfBoundsSideForWorldPos(const IVec2& wc) const {
    if (wc.x < 0 && !WrapsX()) {
        return StageBorderSideKind::Left;
    }
    if (wc.x >= static_cast<int>(GetWidth()) && !WrapsX()) {
        return StageBorderSideKind::Right;
    }
    if (wc.y < 0 && !WrapsY()) {
        return StageBorderSideKind::Top;
    }
    if (wc.y >= static_cast<int>(GetHeight()) && !WrapsY()) {
        return StageBorderSideKind::Bottom;
    }
    return std::nullopt;
}

Tile Stage::GetTileOrBorder(int tile_x, int tile_y) const {
    const IVec2 wrapped = WrapTileCoord(IVec2::New(tile_x, tile_y));
    if (IsTileCoordInside(wrapped.x, wrapped.y)) {
        return GetTile(static_cast<unsigned int>(wrapped.x), static_cast<unsigned int>(wrapped.y));
    }

    const std::optional<StageBorderSideKind> side = GetOutOfBoundsSideForTileCoord(tile_x, tile_y);
    if (!side.has_value()) {
        return Tile::Air;
    }
    return GetBorderTile(*side);
}

bool Stage::IsTileCoordInside(int tile_x, int tile_y) const {
    return tile_x >= 0 && tile_y >= 0 &&
           tile_x < static_cast<int>(GetTileWidth()) &&
           tile_y < static_cast<int>(GetTileHeight());
}

bool Stage::IsWorldPosInside(const IVec2& wc) const {
    return wc.x >= 0 && wc.y >= 0 &&
           wc.x < static_cast<int>(GetWidth()) &&
           wc.y < static_cast<int>(GetHeight());
}

IVec2 Stage::WrapTileCoord(const IVec2& tile_coord) const {
    IVec2 wrapped = tile_coord;
    if (WrapsX()) {
        wrapped.x = WrapCoordinate(wrapped.x, static_cast<int>(GetTileWidth()));
    }
    if (WrapsY()) {
        wrapped.y = WrapCoordinate(wrapped.y, static_cast<int>(GetTileHeight()));
    }
    return wrapped;
}

IVec2 Stage::WrapWorldPos(const IVec2& wc) const {
    IVec2 wrapped = wc;
    if (WrapsX()) {
        wrapped.x = WrapCoordinate(wrapped.x, static_cast<int>(GetWidth()));
    }
    if (WrapsY()) {
        wrapped.y = WrapCoordinate(wrapped.y, static_cast<int>(GetHeight()));
    }
    return wrapped;
}

void Stage::NormalizeEntityPositionForWrap(Entity& entity) const {
    if (WrapsX()) {
        const float stage_width = static_cast<float>(GetWidth());
        while (true) {
            const auto [tl, br] = entity.GetBounds();
            if (br.x < 0.0F) {
                entity.pos.x += stage_width;
                continue;
            }
            if (tl.x >= stage_width) {
                entity.pos.x -= stage_width;
                continue;
            }
            break;
        }
    }

    if (WrapsY()) {
        const float stage_height = static_cast<float>(GetHeight());
        while (true) {
            const auto [tl, br] = entity.GetBounds();
            if (br.y < 0.0F) {
                entity.pos.y += stage_height;
                continue;
            }
            if (tl.y >= stage_height) {
                entity.pos.y -= stage_height;
                continue;
            }
            break;
        }
    }
}

std::pair<UVec2, UVec2> Stage::GetRoomCorners(const UVec2& room) const {
    const UVec2 room_layout_dims = GetRoomLayoutDims();
    const UVec2 room_tile_dims = UVec2::New(
        room_layout_dims.x == 0 ? GetTileWidth() : GetTileWidth() / room_layout_dims.x,
        room_layout_dims.y == 0 ? GetTileHeight() : GetTileHeight() / room_layout_dims.y
    );
    const UVec2 tl = room * room_tile_dims;
    const UVec2 br = tl + room_tile_dims - UVec2::New(1, 1);
    return {tl, br};
}

std::vector<const Tile*> Stage::GetTilesInRoom(const UVec2& room) const {
    const auto [tl, br] = GetRoomCorners(room);
    return GetTilesInRect(ToIVec2(tl), ToIVec2(br));
}

IVec2 Stage::GetStartingRoom() const {
    if (path.empty()) {
        return IVec2::New(0, 0);
    }
    return path.front();
}

IVec2 Stage::GetTileCoordAtWc(const IVec2& wc) const {
    return WrapTileCoord(IVec2::New(
        FloorDiv(wc.x, static_cast<int>(kTileSize)),
        FloorDiv(wc.y, static_cast<int>(kTileSize))
    ));
}

bool Stage::TileCoordAtWcExists(const IVec2& wc) const {
    const IVec2 tile_coord = IVec2::New(
        FloorDiv(wc.x, static_cast<int>(kTileSize)),
        FloorDiv(wc.y, static_cast<int>(kTileSize))
    );
    const bool x_inside = WrapsX() ||
                          (tile_coord.x >= 0 && tile_coord.x < static_cast<int>(GetTileWidth()));
    const bool y_inside = WrapsY() ||
                          (tile_coord.y >= 0 && tile_coord.y < static_cast<int>(GetTileHeight()));
    return x_inside && y_inside;
}

} // namespace splonks

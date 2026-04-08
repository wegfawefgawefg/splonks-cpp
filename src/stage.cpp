#include "stage.hpp"

#include "room.hpp"

#include <algorithm>
#include <random>

namespace splonks {

namespace {

int RandomIntExclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_int_distribution<int> distribution(minimum, maximum - 1);
    return distribution(generator);
}

bool RandomBool() {
    return RandomIntExclusive(0, 2) == 0;
}

unsigned int GetTileRowWidth(const std::vector<std::vector<Tile>>& tiles) {
    if (tiles.empty()) {
        return 0;
    }
    return static_cast<unsigned int>(tiles.front().size());
}

} // namespace

const UVec2 Stage::kShape = UVec2::New(40, 32);
const UVec2 Stage::kRoomShape = UVec2::New(10, 8);
const UVec2 Stage::kRoomLayout = UVec2::New(4, 4);

Stage Stage::NewBlank() {
    Stage stage;
    stage.stage_type = StageType::Blank;
    stage.tiles = std::vector<std::vector<Tile>>(1, std::vector<Tile>(1, Tile::Air));
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    return stage;
}

Stage Stage::New(StageType stage_type) {
    std::vector<std::vector<int>> rooms(
        static_cast<std::size_t>(kRoomLayout.y),
        std::vector<int>(static_cast<std::size_t>(kRoomLayout.x),
                         static_cast<int>(RoomType::Box)));
    std::vector<IVec2> path;

    {
        IVec2 current_room_pos = IVec2::New(
            RandomIntExclusive(0, static_cast<int>(kRoomLayout.x)), 0);

        for (unsigned int floor = 0; floor < kRoomLayout.y; ++floor) {
            const int go_down_x =
                RandomIntExclusive(0, static_cast<int>(kRoomLayout.x));
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

    for (unsigned int room_y = 0; room_y < kRoomLayout.y; ++room_y) {
        for (unsigned int room_x = 0; room_x < kRoomLayout.x; ++room_x) {
            const UVec2 room_pos = UVec2::New(room_x, room_y) * kRoomShape;
            const RoomType room_type =
                static_cast<RoomType>(rooms[static_cast<std::size_t>(room_y)]
                                           [static_cast<std::size_t>(room_x)]);
            const std::vector<std::vector<Tile>> room = GenRoom(room_type, stage_type);

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
    stage.rooms = std::move(rooms);
    stage.path = std::move(path);
    stage.gravity = 0.3F;
    return stage;
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

const Tile* Stage::GetTileAtWc(const IVec2& pos) const {
    if (pos.x < 0 || pos.y < 0) {
        return nullptr;
    }

    const UVec2 upos = ToUVec2(pos);
    const UVec2 stage_dims = GetStageDims();
    if (upos.x >= stage_dims.x || upos.y >= stage_dims.y) {
        return nullptr;
    }

    const UVec2 tile_coords = upos / kTileSize;
    return &GetTile(tile_coords.x, tile_coords.y);
}

std::vector<const Tile*> Stage::GetTilesInRectWc(const IVec2& tl, const IVec2& br) const {
    return GetTilesInRect(tl / static_cast<int>(kTileSize), br / static_cast<int>(kTileSize));
}

std::vector<const Tile*> Stage::GetTilesInRect(const IVec2& tl, const IVec2& br) const {
    if (tl.x >= static_cast<int>(GetTileWidth()) || tl.y >= static_cast<int>(GetTileHeight())) {
        return {};
    }

    const int max_x = static_cast<int>(GetTileWidth()) - 1;
    const int max_y = static_cast<int>(GetTileHeight()) - 1;
    const int start_x = tl.x < 0 ? 0 : tl.x;
    const int start_y = tl.y < 0 ? 0 : tl.y;
    const int end_x = br.x < max_x ? br.x : max_x;
    const int end_y = br.y < max_y ? br.y : max_y;

    std::vector<const Tile*> result;
    for (int y = start_y; y <= end_y; ++y) {
        for (int x = start_x; x <= end_x; ++x) {
            result.push_back(&GetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y)));
        }
    }
    return result;
}

void Stage::SetTile(const IVec2& pos, Tile tile) {
    if (pos.x < 0 || pos.y < 0) {
        return;
    }
    if (pos.x >= static_cast<int>(GetTileWidth()) || pos.y >= static_cast<int>(GetTileHeight())) {
        return;
    }
    tiles[static_cast<std::size_t>(pos.y)][static_cast<std::size_t>(pos.x)] = tile;
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
        static_cast<unsigned int>(RandomIntExclusive(0, static_cast<int>(room_layout_dims.x))),
        static_cast<unsigned int>(RandomIntExclusive(0, static_cast<int>(room_layout_dims.y)))
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
        RandomIntExclusive(0, static_cast<int>(noncollidable_tile_coords.size()));
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
    return wc / static_cast<int>(kTileSize);
}

bool Stage::TileCoordAtWcExists(const IVec2& wc) const {
    const UVec2 stage_dims_wc = GetStageDims();
    if (wc.x < 0 || wc.y < 0) {
        return false;
    }
    if (wc.x >= static_cast<int>(stage_dims_wc.x) || wc.y >= static_cast<int>(stage_dims_wc.y)) {
        return false;
    }
    return true;
}

} // namespace splonks

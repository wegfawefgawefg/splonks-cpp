#include "stage.hpp"

#include "ent.hpp"
#include "room.hpp"
#include "tile_spec.hpp"

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

std::vector<std::vector<EmbeddedTreasure>> MakeEmptyEmbeddedTreasures(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<EmbeddedTreasure>> embedded_treasures;
    embedded_treasures.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        embedded_treasures.push_back(
            std::vector<EmbeddedTreasure>(row.size())
        );
    }
    return embedded_treasures;
}

std::vector<std::vector<float>> MakeEmptyTileShakeGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<float>> tile_shake;
    tile_shake.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        tile_shake.push_back(std::vector<float>(row.size(), 0.0F));
    }
    return tile_shake;
}

std::vector<std::vector<TileRotation>> MakeEmptyTileRotationGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<TileRotation>> tile_rotations;
    tile_rotations.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        tile_rotations.push_back(std::vector<TileRotation>(row.size(), kTileRotation0));
    }
    return tile_rotations;
}

std::vector<std::vector<Vec2>> MakeEmptyFluidVelocityGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<Vec2>> fluid_velocity;
    fluid_velocity.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        fluid_velocity.push_back(std::vector<Vec2>(row.size(), Vec2::New(0.0F, 0.0F)));
    }
    return fluid_velocity;
}

std::vector<std::vector<Vec2>> MakeEmptyFluidVec2Grid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<Vec2>> grid;
    grid.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        grid.push_back(std::vector<Vec2>(row.size(), Vec2::New(0.0F, 0.0F)));
    }
    return grid;
}

std::vector<std::vector<float>> MakeEmptyFluidFloatGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<float>> grid;
    grid.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        grid.push_back(std::vector<float>(row.size(), 0.0F));
    }
    return grid;
}

std::vector<std::vector<Tile>> MakeEmptyFluidTileGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<Tile>> fluid_tiles;
    fluid_tiles.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        fluid_tiles.push_back(std::vector<Tile>(row.size(), Tile::Air));
    }
    return fluid_tiles;
}

std::vector<std::vector<float>> MakeEmptyFluidAmountGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<float>> fluid_amount;
    fluid_amount.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        fluid_amount.push_back(std::vector<float>(row.size(), 0.0F));
    }
    return fluid_amount;
}

std::vector<std::vector<float>> MakeEmptyFluidDisplayAmountGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<float>> fluid_display_amount;
    fluid_display_amount.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        fluid_display_amount.push_back(std::vector<float>(row.size(), 0.0F));
    }
    return fluid_display_amount;
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

using TileShakeGrid = std::vector<std::vector<float>>;

void SyncTileShakeGridToTiles(TileShakeGrid& grid, const std::vector<std::vector<Tile>>& tiles) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyTileShakeGrid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyTileShakeGrid(tiles);
            return;
        }
    }
}

void SyncTileRotationGridToTiles(
    std::vector<std::vector<TileRotation>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyTileRotationGrid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyTileRotationGrid(tiles);
            return;
        }
        for (TileRotation& rotation : grid[y]) {
            rotation &= kTileRotationMask;
        }
    }
}

void SyncFluidVelocityGridToTiles(
    std::vector<std::vector<Vec2>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyFluidVelocityGrid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyFluidVelocityGrid(tiles);
            return;
        }
    }
}

void SyncFluidVec2GridToTiles(
    std::vector<std::vector<Vec2>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyFluidVec2Grid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyFluidVec2Grid(tiles);
            return;
        }
    }
}

void SyncFluidFloatGridToTiles(
    std::vector<std::vector<float>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyFluidFloatGrid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyFluidFloatGrid(tiles);
            return;
        }
    }
}

void SyncFluidTileGridToTiles(
    std::vector<std::vector<Tile>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyFluidTileGrid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyFluidTileGrid(tiles);
            return;
        }
    }
}

void SyncFluidAmountGridToTiles(
    std::vector<std::vector<float>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyFluidAmountGrid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyFluidAmountGrid(tiles);
            return;
        }
    }
}

void SyncFluidDisplayAmountGridToTiles(
    std::vector<std::vector<float>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyFluidDisplayAmountGrid(tiles);
        return;
    }
    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyFluidDisplayAmountGrid(tiles);
            return;
        }
    }
}

std::optional<IVec2> ResolveTileShakeCoord(const Stage& stage, const IVec2& pos) {
    if (stage.tiles.empty() || stage.tiles[0].empty()) {
        return std::nullopt;
    }

    IVec2 resolved = pos;
    const int width = static_cast<int>(stage.GetTileWidth());
    const int height = static_cast<int>(stage.GetTileHeight());
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }

    if (stage.WrapsX()) {
        resolved.x = WrapCoordinate(resolved.x, width);
    } else {
        resolved.x = std::clamp(resolved.x, 0, width - 1);
    }
    if (stage.WrapsY()) {
        resolved.y = WrapCoordinate(resolved.y, height);
    } else {
        resolved.y = std::clamp(resolved.y, 0, height - 1);
    }

    if (!stage.IsTileCoordInside(resolved.x, resolved.y)) {
        return std::nullopt;
    }
    return resolved;
}

void AddTileShakeToGrid(Stage& stage, TileShakeGrid& grid, const IVec2& pos, float amount) {
    SyncTileShakeGridToTiles(grid, stage.tiles);
    if (amount <= 0.0F) {
        return;
    }
    const std::optional<IVec2> resolved = ResolveTileShakeCoord(stage, pos);
    if (!resolved.has_value()) {
        return;
    }
    constexpr float kMaxTileShake = 8.0F;
    float& shake = grid[static_cast<std::size_t>(resolved->y)][static_cast<std::size_t>(resolved->x)];
    shake = std::clamp(shake + amount, 0.0F, kMaxTileShake);
}

void AddTileShakeAreaToGrid(Stage& stage, TileShakeGrid& grid, const IVec2& pos, float magnitude, float dist) {
    SyncTileShakeGridToTiles(grid, stage.tiles);
    if (magnitude <= 0.0F) {
        return;
    }
    if (dist <= 0.0F) {
        AddTileShakeToGrid(stage, grid, pos, magnitude);
        return;
    }
    if (stage.tiles.empty() || stage.tiles[0].empty()) {
        return;
    }

    TileShakeGrid contributions = MakeEmptyTileShakeGrid(stage.tiles);
    std::vector<std::vector<bool>> touched(
        stage.tiles.size(),
        std::vector<bool>(stage.tiles[0].size(), false)
    );

    const int radius = static_cast<int>(std::ceil(dist));
    for (int y = pos.y - radius; y <= pos.y + radius; ++y) {
        for (int x = pos.x - radius; x <= pos.x + radius; ++x) {
            const float dx = static_cast<float>(x - pos.x);
            const float dy = static_cast<float>(y - pos.y);
            const float distance = std::sqrt((dx * dx) + (dy * dy));
            if (distance > dist) {
                continue;
            }
            const float falloff = 1.0F - (distance / dist);
            if (falloff <= 0.0F) {
                continue;
            }

            const std::optional<IVec2> resolved = ResolveTileShakeCoord(stage, IVec2::New(x, y));
            if (!resolved.has_value()) {
                continue;
            }
            float& contribution = contributions[static_cast<std::size_t>(resolved->y)]
                                              [static_cast<std::size_t>(resolved->x)];
            contribution = std::max(contribution, magnitude * falloff);
            touched[static_cast<std::size_t>(resolved->y)][static_cast<std::size_t>(resolved->x)] = true;
        }
    }

    for (std::size_t y = 0; y < contributions.size(); ++y) {
        for (std::size_t x = 0; x < contributions[y].size(); ++x) {
            if (!touched[y][x]) {
                continue;
            }
            constexpr float kMaxTileShake = 8.0F;
            grid[y][x] = std::clamp(grid[y][x] + contributions[y][x], 0.0F, kMaxTileShake);
        }
    }
}

void AttenuateTileShakeGrid(TileShakeGrid& grid, const std::vector<std::vector<Tile>>& tiles, float amount) {
    SyncTileShakeGridToTiles(grid, tiles);
    for (std::vector<float>& row : grid) {
        for (float& shake : row) {
            shake = std::max(0.0F, shake - amount);
        }
    }
}

bool EmbeddedTreasureCoordExists(
    const std::vector<std::vector<EmbeddedTreasure>>& embedded_treasures,
    int tile_x,
    int tile_y
) {
    if (tile_x < 0 || tile_y < 0) {
        return false;
    }
    if (tile_y >= static_cast<int>(embedded_treasures.size())) {
        return false;
    }
    const std::vector<EmbeddedTreasure>& row =
        embedded_treasures[static_cast<std::size_t>(tile_y)];
    return tile_x < static_cast<int>(row.size());
}

} // namespace

bool EmbeddedTreasure::IsEmpty() const {
    for (const EmbeddedTreasureDrop& drop : drops) {
        if (drop.type_ != EntType::None && drop.count > 0) {
            return false;
        }
    }
    return true;
}

bool EmbeddedTreasure::IsVisible() const {
    return visibility == EmbeddedTreasureVisibility::Visible;
}

std::optional<AFrameId> EmbeddedTreasure::GetOverlayFrame() const {
    if (overlay_frame == kInvalidAFrameId) {
        return std::nullopt;
    }
    return overlay_frame;
}

const UVec2 Stage::kShape = UVec2::New(40, 32);
const UVec2 Stage::kRoomShape = UVec2::New(10, 8);
const UVec2 Stage::kRoomLayout = UVec2::New(4, 4);

Stage Stage::NewBlank() {
    Stage stage;
    stage.stage_type = StageType::Blank;
    stage.stage_title = "Blank";
    stage.tiles = std::vector<std::vector<Tile>>(1, std::vector<Tile>(1, Tile::Air));
    stage.tile_rotations = MakeEmptyTileRotationGrid(stage.tiles);
    stage.fluid_tiles = MakeEmptyFluidTileGrid(stage.tiles);
    stage.fluid_amount = MakeEmptyFluidAmountGrid(stage.tiles);
    stage.fluid_display_amount = MakeEmptyFluidDisplayAmountGrid(stage.tiles);
    stage.fluid_velocity = MakeEmptyFluidVelocityGrid(stage.tiles);
    stage.fluid_gravity = MakeEmptyFluidVec2Grid(stage.tiles);
    stage.fluid_gravity_strength = MakeEmptyFluidFloatGrid(stage.tiles);
    stage.fluid_temp_gravity = MakeEmptyFluidVec2Grid(stage.tiles);
    stage.tile_shake = MakeEmptyTileShakeGrid(stage.tiles);
    stage.backwall_tile_shake = MakeEmptyTileShakeGrid(stage.tiles);
    stage.backwall_tiles = MakeEmptyBackwallTiles(stage.tiles);
    stage.backwall_fill_tiles = {};
    stage.embedded_treasures = MakeEmptyEmbeddedTreasures(stage.tiles);
    stage.rooms = {};
    stage.path = {};
    stage.lights = {};
    stage.gravity = kDefaultStageGravity;
    stage.border = MakeUniformBorder(Tile::Air);
    stage.camera_clamp_margin = Vec2::New(0.0F, 0.0F);
    stage.camera_clamp_enabled = true;
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
    AFrameId block_anim_id = aframe_ids::CaveBlock;
    std::string stage_title = "Debug";
    std::vector<Tile> backwall_fill_tiles{
        Tile::CaveAir0,
        Tile::CaveAir1,
        Tile::CaveAir2,
    };
    switch (stage_type) {
    case StageType::Blank:
        stage_title = "Blank";
        border_tile = Tile::CaveDirt;
        break;
    case StageType::Test1:
        stage_title = "Test1";
        border_tile = Tile::CaveDirt;
        break;
    }

    for (unsigned int room_y = 0; room_y < kRoomLayout.y; ++room_y) {
        for (unsigned int room_x = 0; room_x < kRoomLayout.x; ++room_x) {
            const UVec2 room_pos = UVec2::New(room_x, room_y) * kRoomShape;
            const RoomType room_type =
                static_cast<RoomType>(rooms[static_cast<std::size_t>(room_y)]
                                           [static_cast<std::size_t>(room_x)]);
            const RoomTilePalette room_tile_palette{
                .dirt = border_tile,
                .block = Tile::CaveBlock,
            };
            const std::vector<std::vector<Tile>> room =
                GenRoom(room_type, stage_type, room_tile_palette);

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
    stage.stage_title = stage_title;
    stage.block_anim_id = block_anim_id;
    stage.tiles = std::move(tiles);
    stage.tile_rotations = MakeEmptyTileRotationGrid(stage.tiles);
    stage.fluid_tiles = MakeEmptyFluidTileGrid(stage.tiles);
    stage.fluid_amount = MakeEmptyFluidAmountGrid(stage.tiles);
    stage.fluid_display_amount = MakeEmptyFluidDisplayAmountGrid(stage.tiles);
    stage.fluid_velocity = MakeEmptyFluidVelocityGrid(stage.tiles);
    stage.fluid_gravity = MakeEmptyFluidVec2Grid(stage.tiles);
    stage.fluid_gravity_strength = MakeEmptyFluidFloatGrid(stage.tiles);
    stage.fluid_temp_gravity = MakeEmptyFluidVec2Grid(stage.tiles);
    stage.tile_shake = MakeEmptyTileShakeGrid(stage.tiles);
    stage.backwall_tile_shake = MakeEmptyTileShakeGrid(stage.tiles);
    stage.backwall_tiles = MakeEmptyBackwallTiles(stage.tiles);
    stage.FillBackwall(backwall_fill_tiles);
    stage.embedded_treasures = MakeEmptyEmbeddedTreasures(stage.tiles);
    stage.rooms = std::move(rooms);
    stage.path = std::move(path);
    stage.lights = {};
    stage.gravity = kDefaultStageGravity;
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

void Stage::FillBackwall(const std::vector<Tile>& fill_tiles) {
    SyncTileShakeGrid();
    backwall_fill_tiles = fill_tiles;
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

void Stage::SyncTileShakeGrid() {
    SyncTileShakeGridToTiles(tile_shake, tiles);
    SyncTileShakeGridToTiles(backwall_tile_shake, tiles);
}

void Stage::SyncTileInstanceMetadataGrid() {
    SyncTileRotationGridToTiles(tile_rotations, tiles);
    SyncFluidTileGridToTiles(fluid_tiles, tiles);
    SyncFluidAmountGridToTiles(fluid_amount, tiles);
    SyncFluidDisplayAmountGridToTiles(fluid_display_amount, tiles);
    SyncFluidVelocityGridToTiles(fluid_velocity, tiles);
    SyncFluidVec2GridToTiles(fluid_gravity, tiles);
    SyncFluidFloatGridToTiles(fluid_gravity_strength, tiles);
    SyncFluidVec2GridToTiles(fluid_temp_gravity, tiles);
}

void Stage::SyncFluidTileGrid() {
    SyncFluidTileGridToTiles(fluid_tiles, tiles);
}

void Stage::SyncFluidVelocityGrid() {
    SyncFluidVelocityGridToTiles(fluid_velocity, tiles);
}

void Stage::SetFluidGravityOverride(const IVec2& pos, Vec2 gravity_value) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    fluid_gravity[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        gravity_value;
    fluid_gravity_strength[static_cast<std::size_t>(tile_pos.y)]
                          [static_cast<std::size_t>(tile_pos.x)] = 1.0F;
}

void Stage::ClearFluidGravityOverride(const IVec2& pos) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    fluid_gravity[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        Vec2::New(0.0F, 0.0F);
    fluid_gravity_strength[static_cast<std::size_t>(tile_pos.y)]
                          [static_cast<std::size_t>(tile_pos.x)] = 0.0F;
}

void Stage::AddFluidTempGravity(const IVec2& pos, Vec2 gravity_value) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    fluid_temp_gravity[static_cast<std::size_t>(tile_pos.y)]
                      [static_cast<std::size_t>(tile_pos.x)] += gravity_value;
}

void Stage::ClearFluidTempGravity(const IVec2& pos) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    fluid_temp_gravity[static_cast<std::size_t>(tile_pos.y)]
                      [static_cast<std::size_t>(tile_pos.x)] = Vec2::New(0.0F, 0.0F);
}

void Stage::SetTile(const IVec2& pos, Tile tile) {
    SyncTileShakeGrid();
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    if (GetTileSpec(tile).simulated_fluid) {
        SetFluidTile(tile_pos, tile);
        Tile& terrain_tile = tiles[static_cast<std::size_t>(tile_pos.y)]
                                  [static_cast<std::size_t>(tile_pos.x)];
        if (GetTileSpec(terrain_tile).simulated_fluid) {
            terrain_tile = Tile::Air;
            tile_rotations[static_cast<std::size_t>(tile_pos.y)]
                          [static_cast<std::size_t>(tile_pos.x)] = kTileRotation0;
            tile_change_generation += 1;
        }
        return;
    }
    if (tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] == tile) {
        return;
    }
    tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] = tile;
    tile_rotations[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        kTileRotation0;
    if (GetTileSpec(tile).solid || GetTileSpec(tile).one_way_top_solid) {
        fluid_tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
            Tile::Air;
        fluid_amount[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
            0.0F;
        fluid_display_amount[static_cast<std::size_t>(tile_pos.y)]
                            [static_cast<std::size_t>(tile_pos.x)] = 0.0F;
    }
    fluid_velocity[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        Vec2::New(0.0F, 0.0F);
    tile_change_generation += 1;
}

void Stage::SetFluidTile(const IVec2& pos, Tile tile) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    Tile& stored = fluid_tiles[static_cast<std::size_t>(tile_pos.y)]
                              [static_cast<std::size_t>(tile_pos.x)];
    float& amount = fluid_amount[static_cast<std::size_t>(tile_pos.y)]
                                [static_cast<std::size_t>(tile_pos.x)];
    constexpr float max_fluid_amount = 1.0F;
    const float new_amount = GetTileSpec(tile).simulated_fluid ? max_fluid_amount : 0.0F;
    if (stored == tile && amount == new_amount) {
        return;
    }
    stored = tile;
    amount = new_amount;
    fluid_display_amount[static_cast<std::size_t>(tile_pos.y)]
                        [static_cast<std::size_t>(tile_pos.x)] =
                            new_amount;
    fluid_velocity[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        Vec2::New(0.0F, 0.0F);
    tile_change_generation += 1;
}

void Stage::SetTileRotation(const IVec2& pos, TileRotation rotation) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    TileRotation& stored =
        tile_rotations[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)];
    const TileRotation normalized = NormalizeTileRotation(rotation);
    if (stored == normalized) {
        return;
    }
    stored = normalized;
    tile_change_generation += 1;
}

void Stage::AddTileShake(const IVec2& pos, float amount) {
    AddForegroundTileShake(pos, amount);
}

void Stage::AddForegroundTileShake(const IVec2& pos, float amount) {
    AddTileShakeToGrid(*this, tile_shake, pos, amount);
}

void Stage::AddBackgroundTileShake(const IVec2& pos, float amount) {
    AddTileShakeToGrid(*this, backwall_tile_shake, pos, amount);
}

void Stage::AddTileShake(const IVec2& pos, float amount, TileShakeLayerMask layers) {
    if (HasTileShakeLayerMask(layers, TileShakeLayerMask::Foreground)) {
        AddForegroundTileShake(pos, amount);
    }
    if (HasTileShakeLayerMask(layers, TileShakeLayerMask::Background)) {
        AddBackgroundTileShake(pos, amount);
    }
}

void Stage::AddTileShakeArea(const IVec2& pos, float magnitude, float dist) {
    AddForegroundTileShakeArea(pos, magnitude, dist);
}

void Stage::AddForegroundTileShakeArea(const IVec2& pos, float magnitude, float dist) {
    AddTileShakeAreaToGrid(*this, tile_shake, pos, magnitude, dist);
}

void Stage::AddBackgroundTileShakeArea(const IVec2& pos, float magnitude, float dist) {
    AddTileShakeAreaToGrid(*this, backwall_tile_shake, pos, magnitude, dist);
}

void Stage::AddTileShakeArea(const IVec2& pos, float magnitude, float dist, TileShakeLayerMask layers) {
    if (HasTileShakeLayerMask(layers, TileShakeLayerMask::Foreground)) {
        AddForegroundTileShakeArea(pos, magnitude, dist);
    }
    if (HasTileShakeLayerMask(layers, TileShakeLayerMask::Background)) {
        AddBackgroundTileShakeArea(pos, magnitude, dist);
    }
}

void Stage::AttenuateTileShake(float amount) {
    AttenuateTileShake(amount, TileShakeLayerMask::Both);
}

void Stage::AttenuateForegroundTileShake(float amount) {
    AttenuateTileShake(amount, TileShakeLayerMask::Foreground);
}

void Stage::AttenuateBackgroundTileShake(float amount) {
    AttenuateTileShake(amount, TileShakeLayerMask::Background);
}

void Stage::AttenuateTileShake(float amount, TileShakeLayerMask layers) {
    if (HasTileShakeLayerMask(layers, TileShakeLayerMask::Foreground)) {
        AttenuateTileShakeGrid(tile_shake, tiles, amount);
    }
    if (HasTileShakeLayerMask(layers, TileShakeLayerMask::Background)) {
        AttenuateTileShakeGrid(backwall_tile_shake, tiles, amount);
    }
}

void Stage::SetBackwallTile(const IVec2& pos, Tile tile) {
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    backwall_tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] = tile;
}

void Stage::SetEmbeddedTreasure(const IVec2& pos, EntType type_) {
    EmbeddedTreasure embedded_treasure;
    embedded_treasure.drops[0] = EmbeddedTreasureDrop{
        .type_ = type_,
        .count = type_ == EntType::None ? 0 : 1,
    };
    SetEmbeddedTreasure(pos, embedded_treasure);
}

void Stage::SetEmbeddedTreasure(const IVec2& pos, const EmbeddedTreasure& embedded_treasure) {
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    if (!EmbeddedTreasureCoordExists(embedded_treasures, tile_pos.x, tile_pos.y)) {
        return;
    }
    embedded_treasures[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        embedded_treasure;
}

EmbeddedTreasure Stage::TakeEmbeddedTreasure(const IVec2& pos) {
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return EmbeddedTreasure{};
    }
    if (!EmbeddedTreasureCoordExists(embedded_treasures, tile_pos.x, tile_pos.y)) {
        return EmbeddedTreasure{};
    }

    EmbeddedTreasure& treasure =
        embedded_treasures[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)];
    const EmbeddedTreasure result = treasure;
    treasure = EmbeddedTreasure{};
    return result;
}

VID Stage::AddLight(const IVec2& tile_pos, int radius) {
    const VID vid = VID{next_light_vid++};
    return AddLightWithVid(vid, tile_pos, radius);
}

VID Stage::AddLightWithVid(VID vid, const IVec2& tile_pos, int radius) {
    (void)RemoveLight(vid);
    lights.push_back(StageLight{
        .vid = vid,
        .tile_pos = tile_pos,
        .radius = radius,
    });
    next_light_vid = std::max(next_light_vid, vid.id + 1);
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
    SyncTileInstanceMetadataGrid();
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
            tile_rotations[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                kTileRotation0;
        }
    }
    tile_change_generation += 1;
}

} // namespace splonks

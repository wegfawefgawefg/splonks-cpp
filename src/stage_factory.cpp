#include "stage.hpp"

#include "room.hpp"

namespace splonks {

namespace {

bool RandomBool(DetRng& det_rng) {
    return det_rng.RandomIntExclusive(0, 2) == 0;
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

std::vector<std::vector<sim::Scalar>> MakeEmptyTileShakeGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<sim::Scalar>> tile_shake;
    tile_shake.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        tile_shake.push_back(std::vector<sim::Scalar>(row.size(), sim::Scalar::zero()));
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

} // namespace

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
    stage.gravity = sim::ToSimScalar(kDefaultStageGravity);
    stage.border = MakeUniformBorder(Tile::Air);
    stage.camera_clamp_margin = Vec2::New(0.0F, 0.0F);
    stage.camera_clamp_enabled = true;
    return stage;
}

Stage Stage::New(StageType stage_type, DetRng& det_rng) {
    std::vector<std::vector<int>> rooms(
        static_cast<std::size_t>(kRoomLayout.y),
        std::vector<int>(static_cast<std::size_t>(kRoomLayout.x),
                         static_cast<int>(RoomType::Box)));
    std::vector<IVec2> path;

    {
        IVec2 current_room_pos = IVec2::New(
            det_rng.RandomIntExclusive(0, static_cast<int>(kRoomLayout.x)), 0);

        for (unsigned int floor = 0; floor < kRoomLayout.y; ++floor) {
            const int go_down_x =
                det_rng.RandomIntExclusive(0, static_cast<int>(kRoomLayout.x));
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
                GenRoom(room_type, stage_type, room_tile_palette, det_rng);

            const bool flip = RandomBool(det_rng);
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
    stage.FillBackwall(backwall_fill_tiles, det_rng);
    stage.embedded_treasures = MakeEmptyEmbeddedTreasures(stage.tiles);
    stage.rooms = std::move(rooms);
    stage.path = std::move(path);
    stage.lights = {};
    stage.gravity = sim::ToSimScalar(kDefaultStageGravity);
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

} // namespace splonks

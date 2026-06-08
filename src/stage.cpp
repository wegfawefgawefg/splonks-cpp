#include "stage.hpp"

#include "ent.hpp"
#include "tile_spec.hpp"

#include <algorithm>

namespace splonks {

namespace {

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

std::vector<std::vector<sim::Vec2>> MakeEmptyFluidVelocityGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<sim::Vec2>> fluid_velocity;
    fluid_velocity.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        fluid_velocity.push_back(std::vector<sim::Vec2>(row.size(), sim::Vec2::zero()));
    }
    return fluid_velocity;
}

std::vector<std::vector<sim::Vec2>> MakeEmptyFluidVec2Grid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<sim::Vec2>> grid;
    grid.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        grid.push_back(std::vector<sim::Vec2>(row.size(), sim::Vec2::zero()));
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

std::vector<std::vector<std::uint8_t>> MakeEmptyFluidByteGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<std::uint8_t>> grid;
    grid.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        grid.push_back(std::vector<std::uint8_t>(row.size(), 0));
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

std::vector<std::vector<sim::Scalar>> MakeEmptyFluidAmountGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<sim::Scalar>> fluid_amount;
    fluid_amount.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        fluid_amount.push_back(std::vector<sim::Scalar>(row.size(), sim::Scalar::zero()));
    }
    return fluid_amount;
}

std::vector<std::vector<sim::Scalar>> MakeEmptyFluidDisplayAmountGrid(
    const std::vector<std::vector<Tile>>& tiles
) {
    std::vector<std::vector<sim::Scalar>> fluid_display_amount;
    fluid_display_amount.reserve(tiles.size());
    for (const std::vector<Tile>& row : tiles) {
        fluid_display_amount.push_back(std::vector<sim::Scalar>(row.size(), sim::Scalar::zero()));
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
    std::vector<std::vector<sim::Vec2>>& grid,
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
    std::vector<std::vector<sim::Vec2>>& grid,
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

void SyncFluidByteGridToTiles(
    std::vector<std::vector<std::uint8_t>>& grid,
    const std::vector<std::vector<Tile>>& tiles
) {
    if (grid.size() != tiles.size()) {
        grid = MakeEmptyFluidByteGrid(tiles);
        return;
    }

    for (std::size_t y = 0; y < tiles.size(); ++y) {
        if (grid[y].size() != tiles[y].size()) {
            grid = MakeEmptyFluidByteGrid(tiles);
            return;
        }
    }
}

void SyncFluidDisplayAmountGridToTiles(
    std::vector<std::vector<sim::Scalar>>& grid,
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
    std::vector<std::vector<sim::Scalar>>& grid,
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

void Stage::FillBackwall(const std::vector<Tile>& fill_tiles) {
    DetRng fallback_rng = DetRng::New(1);
    FillBackwall(fill_tiles, fallback_rng);
}

void Stage::FillBackwall(const std::vector<Tile>& fill_tiles, DetRng& det_rng) {
    SyncTileShakeGrid();
    backwall_fill_tiles = fill_tiles;
    backwall_tiles = MakeEmptyBackwallTiles(tiles);
    if (fill_tiles.empty()) {
        return;
    }

    for (std::size_t y = 0; y < backwall_tiles.size(); ++y) {
        for (std::size_t x = 0; x < backwall_tiles[y].size(); ++x) {
            const int fill_index =
                det_rng.RandomIntInclusive(0, static_cast<int>(fill_tiles.size()) - 1);
            backwall_tiles[y][x] = fill_tiles[static_cast<std::size_t>(fill_index)];
        }
    }
}

void Stage::SyncTileInstanceMetadataGrid() {
    SyncTileRotationGridToTiles(tile_rotations, tiles);
    SyncFluidTileGridToTiles(fluid_tiles, tiles);
    SyncFluidAmountGridToTiles(fluid_amount, tiles);
    SyncFluidDisplayAmountGridToTiles(fluid_display_amount, tiles);
    SyncFluidVelocityGridToTiles(fluid_velocity, tiles);
    SyncFluidVec2GridToTiles(fluid_gravity, tiles);
    SyncFluidByteGridToTiles(fluid_gravity_strength, tiles);
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
        sim::ToSimVec2(gravity_value);
    fluid_gravity_strength[static_cast<std::size_t>(tile_pos.y)]
                          [static_cast<std::size_t>(tile_pos.x)] = 1;
}

void Stage::ClearFluidGravityOverride(const IVec2& pos) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    fluid_gravity[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        sim::Vec2::zero();
    fluid_gravity_strength[static_cast<std::size_t>(tile_pos.y)]
                          [static_cast<std::size_t>(tile_pos.x)] = 0;
}

void Stage::AddFluidTempGravity(const IVec2& pos, Vec2 gravity_value) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    fluid_temp_gravity[static_cast<std::size_t>(tile_pos.y)]
                      [static_cast<std::size_t>(tile_pos.x)] += sim::ToSimVec2(gravity_value);
}

void Stage::ClearFluidTempGravity(const IVec2& pos) {
    SyncTileInstanceMetadataGrid();
    const IVec2 tile_pos = WrapTileCoord(pos);
    if (!IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    fluid_temp_gravity[static_cast<std::size_t>(tile_pos.y)]
                      [static_cast<std::size_t>(tile_pos.x)] = sim::Vec2::zero();
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
            sim::Scalar::zero();
        fluid_display_amount[static_cast<std::size_t>(tile_pos.y)]
                            [static_cast<std::size_t>(tile_pos.x)] = sim::Scalar::zero();
    }
    fluid_velocity[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        sim::Vec2::zero();
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
    sim::Scalar& amount = fluid_amount[static_cast<std::size_t>(tile_pos.y)]
                                      [static_cast<std::size_t>(tile_pos.x)];
    const sim::Scalar new_amount =
        GetTileSpec(tile).simulated_fluid ? sim::Scalar::from_int(1) : sim::Scalar::zero();
    if (stored == tile && amount == new_amount) {
        return;
    }
    stored = tile;
    amount = new_amount;
    fluid_display_amount[static_cast<std::size_t>(tile_pos.y)]
                        [static_cast<std::size_t>(tile_pos.x)] =
                            new_amount;
    fluid_velocity[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] =
        sim::Vec2::zero();
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
    next_light_vid = std::max(next_light_vid, vid.id + 1U);
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

void Stage::SetTilesInRectWc(sim::AABB area, Tile tile_type) {
    const sim::Scalar tile_size = sim::Scalar::from_int(static_cast<int>(kTileSize));
    SetTilesInRect(
        IAABB::New(
            IVec2::New((area.tl.x / tile_size).floor_int(),
                       (area.tl.y / tile_size).floor_int()),
            IVec2::New((area.br.x / tile_size).floor_int(),
                       (area.br.y / tile_size).floor_int())),
        tile_type);
}

void Stage::SetTilesInRect(IAABB tile_area, Tile tile_type) {
    SyncTileInstanceMetadataGrid();
    const int max_x = static_cast<int>(GetTileWidth()) - 1;
    const int max_y = static_cast<int>(GetTileHeight()) - 1;
    const IVec2 tl = tile_area.tl;
    const IVec2 br = tile_area.br;

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

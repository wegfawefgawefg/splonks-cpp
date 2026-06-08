#include "stage.hpp"

#include <algorithm>

namespace splonks {

namespace {

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

float GetTileShakeFromGrid(const std::vector<std::vector<sim::Scalar>>& grid,
                           std::uint32_t x,
                           std::uint32_t y) {
    if (y >= grid.size()) {
        return 0.0F;
    }
    const std::vector<sim::Scalar>& row = grid[static_cast<std::size_t>(y)];
    if (x >= row.size()) {
        return 0.0F;
    }
    return sim::ToRenderScalar(row[static_cast<std::size_t>(x)]);
}

bool EmbeddedTreasureCoordExists(
    const std::vector<std::vector<EmbeddedTreasure>>& embedded_treasures,
    int x,
    int y) {
    if (y < 0 || y >= static_cast<int>(embedded_treasures.size())) {
        return false;
    }
    const std::vector<EmbeddedTreasure>& row = embedded_treasures[static_cast<std::size_t>(y)];
    return x >= 0 && x < static_cast<int>(row.size());
}

} // namespace

UVec2 Stage::GetStageDims() const {
    return UVec2::New(GetTileWidth(), GetTileHeight()) * kTileSize;
}

UVec2 Stage::GetRoomLayoutDims() const {
    if (rooms.empty() || rooms.front().empty()) {
        return UVec2::New(1, 1);
    }
    return UVec2::New(
        static_cast<std::uint32_t>(rooms.front().size()),
        static_cast<std::uint32_t>(rooms.size())
    );
}

UVec2 Stage::GetRegularRoomGridRoomDims() const {
    const UVec2 room_layout_dims = GetRoomLayoutDims();
    if (room_layout_dims.x == 0 || room_layout_dims.y == 0) {
        return GetStageDims();
    }
    return UVec2::New(
        GetTileWidth() / room_layout_dims.x,
        GetTileHeight() / room_layout_dims.y
    ) * kTileSize;
}

IVec2 Stage::GetRegularRoomGridTlWc(const IVec2& room) const {
    const UVec2 room_dims = GetRegularRoomGridRoomDims();
    return IVec2::New(
        room.x * static_cast<int>(room_dims.x),
        room.y * static_cast<int>(room_dims.y)
    );
}

const Tile& Stage::GetTile(std::uint32_t x, std::uint32_t y) const {
    return tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

TileRotation Stage::GetTileRotation(std::uint32_t x, std::uint32_t y) const {
    if (y >= tile_rotations.size()) {
        return kTileRotation0;
    }
    const std::vector<TileRotation>& row = tile_rotations[static_cast<std::size_t>(y)];
    if (x >= row.size()) {
        return kTileRotation0;
    }
    return row[static_cast<std::size_t>(x)] & kTileRotationMask;
}

Tile Stage::GetFluidTile(std::uint32_t x, std::uint32_t y) const {
    if (y >= fluid_tiles.size()) {
        return Tile::Air;
    }
    const std::vector<Tile>& row = fluid_tiles[static_cast<std::size_t>(y)];
    if (x >= row.size()) {
        return Tile::Air;
    }
    return row[static_cast<std::size_t>(x)];
}

float Stage::GetFluidAmount(std::uint32_t x, std::uint32_t y) const {
    if (y >= fluid_amount.size()) {
        return 0.0F;
    }
    const std::vector<sim::Scalar>& row = fluid_amount[static_cast<std::size_t>(y)];
    if (x >= row.size()) {
        return 0.0F;
    }
    return sim::ToRenderScalar(row[static_cast<std::size_t>(x)]);
}

float Stage::GetTileShake(std::uint32_t x, std::uint32_t y) const {
    return GetForegroundTileShake(x, y);
}

float Stage::GetForegroundTileShake(std::uint32_t x, std::uint32_t y) const {
    return GetTileShakeFromGrid(tile_shake, x, y);
}

float Stage::GetBackgroundTileShake(std::uint32_t x, std::uint32_t y) const {
    return GetTileShakeFromGrid(backwall_tile_shake, x, y);
}

const Tile& Stage::GetBackwallTile(std::uint32_t x, std::uint32_t y) const {
    return backwall_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

EmbeddedTreasure Stage::GetEmbeddedTreasure(std::uint32_t x, std::uint32_t y) const {
    if (!EmbeddedTreasureCoordExists(
            embedded_treasures,
            static_cast<int>(x),
            static_cast<int>(y))) {
        return EmbeddedTreasure{};
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

    return &GetTile(static_cast<std::uint32_t>(tile_coords.x), static_cast<std::uint32_t>(tile_coords.y));
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
            result.push_back(&GetTile(static_cast<std::uint32_t>(tile_pos.x), static_cast<std::uint32_t>(tile_pos.y)));
        }
    }
    return result;
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
            const Tile tile = GetTile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
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

UVec2 Stage::GetRandomRegularRoomGridCoord(DetRng& det_rng) const {
    const UVec2 room_layout_dims = GetRoomLayoutDims();
    return UVec2::New(
        static_cast<std::uint32_t>(det_rng.RandomIntExclusive(0, static_cast<int>(room_layout_dims.x))),
        static_cast<std::uint32_t>(det_rng.RandomIntExclusive(0, static_cast<int>(room_layout_dims.y)))
    );
}

std::optional<IVec2> Stage::GetRandomNoncollidablePositionInStage(DetRng& det_rng) const {
    std::vector<IVec2> noncollidable_tile_coords;
    for (int y = 0; y < static_cast<int>(GetTileHeight()); ++y) {
        for (int x = 0; x < static_cast<int>(GetTileWidth()); ++x) {
            const Tile tile = GetTile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
            if (!IsTileCollidable(tile)) {
                noncollidable_tile_coords.push_back(IVec2::New(x, y));
            }
        }
    }
    if (noncollidable_tile_coords.empty()) {
        return std::nullopt;
    }
    const int random_tile_idx =
        det_rng.RandomIntExclusive(0, static_cast<int>(noncollidable_tile_coords.size()));
    const IVec2 tile_coord = noncollidable_tile_coords[static_cast<std::size_t>(random_tile_idx)];
    return tile_coord * static_cast<int>(kTileSize);
}

std::optional<IVec2> Stage::GetRandomNoncollidablePositionInRandomRegularRoomGridCell(
    DetRng& det_rng) const {
    const UVec2 random_room = GetRandomRegularRoomGridCoord(det_rng);
    return GetRandomNoncollidablePositionInRegularRoomGridCell(random_room, det_rng);
}

std::optional<IVec2> Stage::GetRandomNoncollidablePositionInRegularRoomGridCell(
    const UVec2& room, DetRng& det_rng) const {
    const auto [room_tl, room_br] = GetRegularRoomGridCorners(room);

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
            const Tile tile = GetTile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
            if (!IsTileCollidable(tile)) {
                noncollidable_tile_coords.push_back(IVec2::New(x, y));
            }
        }
    }

    if (noncollidable_tile_coords.empty()) {
        return std::nullopt;
    }

    const int random_tile_idx =
        det_rng.RandomIntExclusive(0, static_cast<int>(noncollidable_tile_coords.size()));
    const IVec2 tile_coord = noncollidable_tile_coords[static_cast<std::size_t>(random_tile_idx)];
    return tile_coord * static_cast<int>(kTileSize);
}

std::pair<UVec2, UVec2> Stage::GetRegularRoomGridCorners(const UVec2& room) const {
    const UVec2 room_layout_dims = GetRoomLayoutDims();
    const UVec2 room_tile_dims = UVec2::New(
        room_layout_dims.x == 0 ? GetTileWidth() : GetTileWidth() / room_layout_dims.x,
        room_layout_dims.y == 0 ? GetTileHeight() : GetTileHeight() / room_layout_dims.y
    );
    const UVec2 tl = room * room_tile_dims;
    const UVec2 br = tl + room_tile_dims - UVec2::New(1, 1);
    return {tl, br};
}

std::vector<const Tile*> Stage::GetTilesInRegularRoomGridCell(const UVec2& room) const {
    const auto [tl, br] = GetRegularRoomGridCorners(room);
    return GetTilesInRect(ToIVec2(tl), ToIVec2(br));
}

IVec2 Stage::GetStartingRoom() const {
    if (path.empty()) {
        return IVec2::New(0, 0);
    }
    return path.front();
}

} // namespace splonks

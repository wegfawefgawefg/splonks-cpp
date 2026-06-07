#include "stage.hpp"

#include "ent.hpp"

namespace splonks {

namespace {

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

std::uint32_t GetTileRowWidth(const std::vector<std::vector<Tile>>& tiles) {
    if (tiles.empty()) {
        return 0;
    }
    return static_cast<std::uint32_t>(tiles[0].size());
}

} // namespace

std::uint32_t Stage::GetWidth() const {
    return GetTileWidth() * kTileSize;
}

std::uint32_t Stage::GetHeight() const {
    return GetTileHeight() * kTileSize;
}

std::uint32_t Stage::GetTileWidth() const {
    return GetTileRowWidth(tiles);
}

std::uint32_t Stage::GetTileHeight() const {
    return static_cast<std::uint32_t>(tiles.size());
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
        return GetTile(static_cast<std::uint32_t>(wrapped.x), static_cast<std::uint32_t>(wrapped.y));
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

StageExitId Stage::FindExitId(std::string_view id) const {
    for (std::size_t i = 0; i < exits.size(); ++i) {
        if (exits[i].id == id) {
            return static_cast<StageExitId>(i);
        }
    }
    return kInvalidStageExitId;
}

const StageExit* Stage::GetExit(StageExitId id) const {
    if (id < 0 || static_cast<std::size_t>(id) >= exits.size()) {
        return nullptr;
    }
    return &exits[static_cast<std::size_t>(id)];
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

void Stage::NormalizeEntPositionForWrap(Ent& ent) const {
    if (WrapsX()) {
        const float stage_width = static_cast<float>(GetWidth());
        while (true) {
            const auto [tl, br] = ent.GetBounds();
            if (br.x < 0.0F) {
                ent.pos.x += stage_width;
                continue;
            }
            if (tl.x >= stage_width) {
                ent.pos.x -= stage_width;
                continue;
            }
            break;
        }
    }

    if (WrapsY()) {
        const float stage_height = static_cast<float>(GetHeight());
        while (true) {
            const auto [tl, br] = ent.GetBounds();
            if (br.y < 0.0F) {
                ent.pos.y += stage_height;
                continue;
            }
            if (tl.y >= stage_height) {
                ent.pos.y -= stage_height;
                continue;
            }
            break;
        }
    }
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

#include "stage.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace splonks {

namespace {

using TileShakeGrid = std::vector<std::vector<float>>;

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

} // namespace

void Stage::SyncTileShakeGrid() {
    SyncTileShakeGridToTiles(tile_shake, tiles);
    SyncTileShakeGridToTiles(backwall_tile_shake, tiles);
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

} // namespace splonks

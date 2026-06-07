#include "stage_acoustics.hpp"

#include "state.hpp"
#include "tile.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>

namespace splonks {

namespace {

const std::array<IVec2, 8> kOpennessRayDirections{{
    IVec2::New(1, 0),
    IVec2::New(1, 1),
    IVec2::New(0, 1),
    IVec2::New(-1, 1),
    IVec2::New(-1, 0),
    IVec2::New(-1, -1),
    IVec2::New(0, -1),
    IVec2::New(1, -1),
}};

std::vector<std::vector<float>> MakeEmptyOpennessGrid(const Stage& stage) {
    return std::vector<std::vector<float>>(
        static_cast<std::size_t>(stage.GetTileHeight()),
        std::vector<float>(static_cast<std::size_t>(stage.GetTileWidth()), 0.0F)
    );
}

void EnsureStageAcousticsCacheShape(State& state) {
    const std::size_t tile_height = static_cast<std::size_t>(state.stage.GetTileHeight());
    const std::size_t tile_width = static_cast<std::size_t>(state.stage.GetTileWidth());
    const bool shape_matches =
        state.stage_acoustics.openness.tiles.size() == tile_height &&
        (tile_height == 0 ||
         state.stage_acoustics.openness.tiles.front().size() == tile_width);
    if (shape_matches) {
        return;
    }

    state.stage_acoustics.openness.tiles = MakeEmptyOpennessGrid(state.stage);
    state.stage_acoustics.openness.valid = false;
}

float ComputeTileOpenness(const Stage& stage, int tile_x, int tile_y) {
    if (!stage.IsTileCoordInside(tile_x, tile_y)) {
        return 0.0F;
    }

    const Tile tile = stage.GetTile(
        static_cast<unsigned int>(tile_x),
        static_cast<unsigned int>(tile_y)
    );
    if (IsTileCollidable(tile)) {
        return 0.0F;
    }

    const IVec2 origin_tile = IVec2::New(tile_x, tile_y);
    float openness_sum = 0.0F;
    for (const IVec2& direction : kOpennessRayDirections) {
        openness_sum += CastStageOpennessRay(stage, origin_tile, direction).openness;
    }
    return openness_sum / static_cast<float>(kOpennessRayDirections.size());
}

} // namespace

StageOpennessRay CastStageOpennessRay(
    const Stage& stage,
    const IVec2& origin_tile,
    const IVec2& direction
) {
    const TileStepRaycastResult ray = RaycastTileSteps(
        stage,
        origin_tile,
        direction,
        kStageOpennessRayLengthTiles
    );

    StageOpennessRay result;
    result.last_open_tile = ray.last_open_tile;
    result.last_open_unwrapped_tile = ray.last_open_unwrapped_tile;
    result.blocker_tile = ray.blocker_tile;
    result.blocker_unwrapped_tile = ray.blocker_unwrapped_tile;
    result.blocked = ray.blocked;

    result.openness = kStageOpennessRayLengthTiles > 0
        ? std::clamp(
              static_cast<float>(ray.open_steps) /
                  static_cast<float>(kStageOpennessRayLengthTiles),
              0.0F,
              1.0F
          )
        : 0.0F;
    return result;
}

StageOpennessCache StageOpennessCache::New() {
    return StageOpennessCache{};
}

StageAcoustics StageAcoustics::New() {
    StageAcoustics acoustics;
    acoustics.openness = StageOpennessCache::New();
    return acoustics;
}

void InvalidateStageAcoustics(State& state) {
    state.stage_acoustics.openness.valid = false;
}

void RebuildStageAcoustics(State& state) {
    EnsureStageAcousticsCacheShape(state);

    for (std::size_t y = 0; y < state.stage_acoustics.openness.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage_acoustics.openness.tiles[y].size(); ++x) {
            state.stage_acoustics.openness.tiles[y][x] = ComputeTileOpenness(
                state.stage,
                static_cast<int>(x),
                static_cast<int>(y)
            );
        }
    }

    state.stage_acoustics.openness.valid = true;
}

void EnsureStageAcoustics(State& state) {
    EnsureStageAcousticsCacheShape(state);
    if (!state.stage_acoustics.openness.valid) {
        RebuildStageAcoustics(state);
    }
}

void UpdateStageAcousticsForTileChange(State& state, const IVec2& tile_pos) {
    const std::vector<IVec2> tile_positions{tile_pos};
    UpdateStageAcousticsForTileChanges(state, tile_positions);
}

void UpdateStageAcousticsForTileChanges(
    State& state,
    const std::vector<IVec2>& tile_positions
) {
    if (tile_positions.empty()) {
        return;
    }

    EnsureStageAcousticsCacheShape(state);
    if (!state.stage_acoustics.openness.valid) {
        RebuildStageAcoustics(state);
        return;
    }

    const std::uint32_t tile_width = state.stage.GetTileWidth();
    const std::uint32_t tile_height = state.stage.GetTileHeight();
    if (tile_width == 0 || tile_height == 0) {
        return;
    }

    std::vector<bool> touched(
        static_cast<std::size_t>(tile_width) * static_cast<std::size_t>(tile_height),
        false
    );

    for (const IVec2& changed_tile : tile_positions) {
        for (int y = changed_tile.y - kStageOpennessRayLengthTiles;
             y <= changed_tile.y + kStageOpennessRayLengthTiles;
             ++y) {
            for (int x = changed_tile.x - kStageOpennessRayLengthTiles;
                 x <= changed_tile.x + kStageOpennessRayLengthTiles;
                 ++x) {
                const IVec2 wrapped = state.stage.WrapTileCoord(IVec2::New(x, y));
                if (!state.stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
                    continue;
                }

                const std::size_t index =
                    static_cast<std::size_t>(wrapped.y) * static_cast<std::size_t>(tile_width) +
                    static_cast<std::size_t>(wrapped.x);
                if (touched[index]) {
                    continue;
                }
                touched[index] = true;

                state.stage_acoustics.openness.tiles[static_cast<std::size_t>(wrapped.y)]
                                                  [static_cast<std::size_t>(wrapped.x)] =
                    ComputeTileOpenness(state.stage, wrapped.x, wrapped.y);
            }
        }
    }
}

float GetStageTileOpenness(const State& state, int tile_x, int tile_y) {
    if (tile_x >= 0 && tile_y >= 0 &&
        tile_x < static_cast<int>(state.stage_acoustics.openness.tiles.empty()
                                      ? 0
                                      : state.stage_acoustics.openness.tiles.front().size()) &&
        tile_y < static_cast<int>(state.stage_acoustics.openness.tiles.size()) &&
        state.stage_acoustics.openness.valid) {
        return state.stage_acoustics.openness.tiles[static_cast<std::size_t>(tile_y)]
                                                [static_cast<std::size_t>(tile_x)];
    }

    return ComputeTileOpenness(state.stage, tile_x, tile_y);
}

} // namespace splonks

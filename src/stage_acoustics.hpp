#pragma once

#include "math_types.hpp"

#include <optional>
#include <vector>

namespace splonks {

constexpr int kStageOpennessRayLengthTiles = 12;

struct StageOpennessRay {
    IVec2 last_open_tile = IVec2::New(0, 0);
    IVec2 last_open_unwrapped_tile = IVec2::New(0, 0);
    std::optional<IVec2> blocker_tile = std::nullopt;
    IVec2 blocker_unwrapped_tile = IVec2::New(0, 0);
    bool blocked = false;
    float openness = 0.0F;
};

struct StageOpennessCache {
    std::vector<std::vector<float>> tiles;
    bool valid = false;

    static StageOpennessCache New();
};

struct StageAcoustics {
    StageOpennessCache openness;

    static StageAcoustics New();
};

struct Stage;
struct State;

StageOpennessRay CastStageOpennessRay(
    const Stage& stage,
    const IVec2& origin_tile,
    const IVec2& direction
);
void InvalidateStageAcoustics(State& state);
void RebuildStageAcoustics(State& state);
void EnsureStageAcoustics(State& state);
void UpdateStageAcousticsForTileChange(State& state, const IVec2& tile_pos);
void UpdateStageAcousticsForTileChanges(
    State& state,
    const std::vector<IVec2>& tile_positions
);
float GetStageTileOpenness(const State& state, int tile_x, int tile_y);

} // namespace splonks

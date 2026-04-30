#pragma once

#include "stage.hpp"

#include <vector>

namespace splonks::debug_stage {

inline constexpr Tile kDefaultBorderTile = Tile::CaveDirt;

inline void FillBackwall(Stage& stage) {
    stage.FillBackwall(std::vector<Tile>{
        Tile::CaveAir0,
        Tile::CaveAir1,
        Tile::CaveAir2,
    });
}

inline void SetTile(Stage& stage, int x, int y, Tile tile) {
    stage.SetTile(IVec2::New(x, y), tile);
}

inline void FillRect(Stage& stage, int left_x, int top_y, int right_x, int bottom_y, Tile tile) {
    for (int y = top_y; y <= bottom_y; ++y) {
        for (int x = left_x; x <= right_x; ++x) {
            SetTile(stage, x, y, tile);
        }
    }
}

inline void CarveRect(Stage& stage, int left_x, int top_y, int right_x, int bottom_y) {
    FillRect(stage, left_x, top_y, right_x, bottom_y, Tile::Air);
}

inline void ApplyDefaultDebugCamera(Stage& stage) {
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;
}

inline void BuildLadder(Stage& stage, int x, int top_y, int bottom_y) {
    if (top_y > bottom_y) {
        return;
    }

    SetTile(stage, x, top_y, Tile::LadderTop);
    for (int y = top_y + 1; y <= bottom_y; ++y) {
        SetTile(stage, x, y, Tile::Ladder);
    }
}

} // namespace splonks::debug_stage

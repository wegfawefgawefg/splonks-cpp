#include "terrain_lighting.hpp"

#include "graphics.hpp"
#include "state.hpp"
#include "tile.hpp"

#include <algorithm>

namespace splonks {

namespace {

constexpr int kTileChangeUpdateRadius = 2;

bool IsTerrainLightingTile(Tile tile) {
    switch (tile) {
    case Tile::Dirt:
    case Tile::Gold:
    case Tile::Block:
        return true;
    default:
        return false;
    }
}

bool StageTileExists(const State& state, int tile_x, int tile_y) {
    return tile_x >= 0 && tile_y >= 0 &&
           tile_x < static_cast<int>(state.stage.GetTileWidth()) &&
           tile_y < static_cast<int>(state.stage.GetTileHeight());
}

Tile GetTileForLighting(const State& state, int tile_x, int tile_y) {
    if (!StageTileExists(state, tile_x, tile_y)) {
        return state.settings.post_process.terrain_face_enclosed_stage_bounds ? Tile::Dirt
                                                                              : Tile::Air;
    }

    return state.stage.tiles[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)];
}

bool IsBackwallLightingTile(const State& state, int tile_x, int tile_y) {
    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    return !IsTerrainLightingTile(tile);
}

float GetRawTileOpenness(const State& state, int tile_x, int tile_y) {
    const Tile center_tile = GetTileForLighting(state, tile_x, tile_y);
    if (!IsTerrainLightingTile(center_tile)) {
        return 0.5F;
    }

    const float diagonal_weight = std::clamp(
        state.settings.post_process.terrain_exposure_diagonal_weight,
        0.0F,
        1.0F
    );

    float openness = 0.0F;
    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x, tile_y - 1))) {
        openness += 1.0F;
    }
    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x, tile_y + 1))) {
        openness += 1.0F;
    }
    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x - 1, tile_y))) {
        openness += 1.0F;
    }
    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x + 1, tile_y))) {
        openness += 1.0F;
    }

    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x - 1, tile_y - 1))) {
        openness += diagonal_weight;
    }
    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x + 1, tile_y - 1))) {
        openness += diagonal_weight;
    }
    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x - 1, tile_y + 1))) {
        openness += diagonal_weight;
    }
    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x + 1, tile_y + 1))) {
        openness += diagonal_weight;
    }

    const float max_openness = 4.0F + (4.0F * diagonal_weight);
    return std::clamp(openness / max_openness, 0.0F, 1.0F);
}

float GetSmoothedTileOpenness(const State& state, int tile_x, int tile_y) {
    const float raw_openness = GetRawTileOpenness(state, tile_x, tile_y);
    const float smoothing = std::clamp(state.settings.post_process.terrain_exposure_smoothing, 0.0F, 1.0F);
    if (smoothing <= 0.0F) {
        return raw_openness;
    }

    float openness_sum = 0.0F;
    int samples = 0;
    for (int sample_y = tile_y - 1; sample_y <= tile_y + 1; ++sample_y) {
        for (int sample_x = tile_x - 1; sample_x <= tile_x + 1; ++sample_x) {
            if (!IsTerrainLightingTile(GetTileForLighting(state, sample_x, sample_y))) {
                continue;
            }
            openness_sum += GetRawTileOpenness(state, sample_x, sample_y);
            samples += 1;
        }
    }

    if (samples == 0) {
        return raw_openness;
    }

    const float neighborhood_average = openness_sum / static_cast<float>(samples);
    return std::lerp(raw_openness, neighborhood_average, smoothing);
}

float GetRawBackwallOpenness(const State& state, int tile_x, int tile_y) {
    if (!IsBackwallLightingTile(state, tile_x, tile_y)) {
        return 0.5F;
    }

    const float diagonal_weight = std::clamp(
        state.settings.post_process.terrain_exposure_diagonal_weight,
        0.0F,
        1.0F
    );

    float openness = 0.0F;
    if (IsBackwallLightingTile(state, tile_x, tile_y - 1)) {
        openness += 1.0F;
    }
    if (IsBackwallLightingTile(state, tile_x, tile_y + 1)) {
        openness += 1.0F;
    }
    if (IsBackwallLightingTile(state, tile_x - 1, tile_y)) {
        openness += 1.0F;
    }
    if (IsBackwallLightingTile(state, tile_x + 1, tile_y)) {
        openness += 1.0F;
    }

    if (IsBackwallLightingTile(state, tile_x - 1, tile_y - 1)) {
        openness += diagonal_weight;
    }
    if (IsBackwallLightingTile(state, tile_x + 1, tile_y - 1)) {
        openness += diagonal_weight;
    }
    if (IsBackwallLightingTile(state, tile_x - 1, tile_y + 1)) {
        openness += diagonal_weight;
    }
    if (IsBackwallLightingTile(state, tile_x + 1, tile_y + 1)) {
        openness += diagonal_weight;
    }

    const float max_openness = 4.0F + (4.0F * diagonal_weight);
    return std::clamp(openness / max_openness, 0.0F, 1.0F);
}

float GetSmoothedBackwallOpenness(const State& state, int tile_x, int tile_y) {
    const float raw_openness = GetRawBackwallOpenness(state, tile_x, tile_y);
    const float smoothing = std::clamp(state.settings.post_process.backwall_smoothing, 0.0F, 1.0F);
    if (smoothing <= 0.0F) {
        return raw_openness;
    }

    float openness_sum = 0.0F;
    int samples = 0;
    for (int sample_y = tile_y - 1; sample_y <= tile_y + 1; ++sample_y) {
        for (int sample_x = tile_x - 1; sample_x <= tile_x + 1; ++sample_x) {
            if (!IsBackwallLightingTile(state, sample_x, sample_y)) {
                continue;
            }
            openness_sum += GetRawBackwallOpenness(state, sample_x, sample_y);
            samples += 1;
        }
    }

    if (samples == 0) {
        return raw_openness;
    }

    const float neighborhood_average = openness_sum / static_cast<float>(samples);
    return std::lerp(raw_openness, neighborhood_average, smoothing);
}

float ComputeTileBrightness(const State& state, int tile_x, int tile_y) {
    if (!state.settings.post_process.terrain_exposure_lighting) {
        return 1.0F;
    }

    if (!IsTerrainLightingTile(GetTileForLighting(state, tile_x, tile_y))) {
        return 1.0F;
    }

    const float openness = GetSmoothedTileOpenness(state, tile_x, tile_y);
    const float amount = std::clamp(state.settings.post_process.terrain_exposure_amount, 0.0F, 1.0F);
    const float min_brightness = std::clamp(
        state.settings.post_process.terrain_exposure_min_brightness,
        0.0F,
        2.0F
    );
    const float max_brightness = std::clamp(
        state.settings.post_process.terrain_exposure_max_brightness,
        0.0F,
        2.0F
    );
    const float low = std::min(min_brightness, max_brightness);
    const float high = std::max(min_brightness, max_brightness);
    const float target_brightness = std::lerp(low, high, openness);
    return std::clamp(std::lerp(1.0F, target_brightness, amount), 0.0F, 2.0F);
}

float ComputeBackwallBrightness(const State& state, int tile_x, int tile_y) {
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.backwall_lighting) {
        return 1.0F;
    }

    if (!IsBackwallLightingTile(state, tile_x, tile_y)) {
        return 1.0F;
    }

    const float openness = GetSmoothedBackwallOpenness(state, tile_x, tile_y);
    const float min_brightness = std::clamp(
        state.settings.post_process.backwall_min_brightness,
        0.0F,
        2.0F
    );
    const float max_brightness = std::clamp(
        state.settings.post_process.backwall_max_brightness,
        0.0F,
        2.0F
    );
    const float low = std::min(min_brightness, max_brightness);
    const float high = std::max(min_brightness, max_brightness);
    const float brightness_scale = std::clamp(
        state.settings.post_process.backwall_brightness,
        0.0F,
        2.0F
    );
    return std::clamp(std::lerp(low, high, openness) * brightness_scale, 0.0F, 2.0F);
}

TerrainLightingTile BuildTerrainLightingTile(const State& state, int tile_x, int tile_y) {
    TerrainLightingTile result;

    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    if (!IsTerrainLightingTile(tile)) {
        return result;
    }

    result.open_top = !IsTerrainLightingTile(GetTileForLighting(state, tile_x, tile_y - 1));
    result.open_bottom = !IsTerrainLightingTile(GetTileForLighting(state, tile_x, tile_y + 1));
    result.open_left = !IsTerrainLightingTile(GetTileForLighting(state, tile_x - 1, tile_y));
    result.open_right = !IsTerrainLightingTile(GetTileForLighting(state, tile_x + 1, tile_y));

    const bool open_top_left =
        !IsTerrainLightingTile(GetTileForLighting(state, tile_x - 1, tile_y - 1));
    const bool open_top_right =
        !IsTerrainLightingTile(GetTileForLighting(state, tile_x + 1, tile_y - 1));
    const bool open_bottom_left =
        !IsTerrainLightingTile(GetTileForLighting(state, tile_x - 1, tile_y + 1));
    const bool open_bottom_right =
        !IsTerrainLightingTile(GetTileForLighting(state, tile_x + 1, tile_y + 1));

    result.ao_top_left = !result.open_top && !result.open_left && open_top_left;
    result.ao_top_right = !result.open_top && !result.open_right && open_top_right;
    result.ao_bottom_left = !result.open_bottom && !result.open_left && open_bottom_left;
    result.ao_bottom_right = !result.open_bottom && !result.open_right && open_bottom_right;
    result.brightness = ComputeTileBrightness(state, tile_x, tile_y);

    return result;
}

BackwallLightingTile BuildBackwallLightingTile(const State& state, int tile_x, int tile_y) {
    BackwallLightingTile result;
    result.brightness = ComputeBackwallBrightness(state, tile_x, tile_y);
    return result;
}

void EnsureTerrainLightingCacheShape(const State& state, Graphics& graphics) {
    const std::size_t tile_height = state.stage.tiles.size();
    const std::size_t tile_width =
        tile_height == 0 ? 0 : state.stage.tiles.front().size();
    const bool terrain_shape_matches =
        graphics.terrain_lighting_cache.tiles.size() == tile_height &&
        (tile_height == 0 ||
         graphics.terrain_lighting_cache.tiles.front().size() == tile_width);
    const bool backwall_shape_matches =
        graphics.backwall_lighting_cache.tiles.size() == tile_height &&
        (tile_height == 0 ||
         graphics.backwall_lighting_cache.tiles.front().size() == tile_width);
    if (terrain_shape_matches && backwall_shape_matches) {
        return;
    }

    graphics.terrain_lighting_cache.tiles.assign(
        tile_height,
        std::vector<TerrainLightingTile>(tile_width)
    );
    graphics.backwall_lighting_cache.tiles.assign(
        tile_height,
        std::vector<BackwallLightingTile>(tile_width)
    );
    graphics.terrain_lighting_cache.valid = false;
    graphics.backwall_lighting_cache.valid = false;
}

} // namespace

TerrainLightingCache TerrainLightingCache::New() {
    return TerrainLightingCache{};
}

BackwallLightingCache BackwallLightingCache::New() {
    return BackwallLightingCache{};
}

void InvalidateTerrainLightingCache(Graphics& graphics) {
    graphics.terrain_lighting_cache.valid = false;
    graphics.backwall_lighting_cache.valid = false;
}

void RebuildTerrainLightingCache(const State& state, Graphics& graphics) {
    EnsureTerrainLightingCacheShape(state, graphics);

    for (std::size_t y = 0; y < graphics.terrain_lighting_cache.tiles.size(); ++y) {
        for (std::size_t x = 0; x < graphics.terrain_lighting_cache.tiles[y].size(); ++x) {
            graphics.terrain_lighting_cache.tiles[y][x] = BuildTerrainLightingTile(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            graphics.backwall_lighting_cache.tiles[y][x] = BuildBackwallLightingTile(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
        }
    }

    graphics.terrain_lighting_cache.valid = true;
    graphics.backwall_lighting_cache.valid = true;
}

void EnsureTerrainLightingCache(const State& state, Graphics& graphics) {
    EnsureTerrainLightingCacheShape(state, graphics);
    if (!graphics.terrain_lighting_cache.valid || !graphics.backwall_lighting_cache.valid) {
        RebuildTerrainLightingCache(state, graphics);
    }
}

void UpdateTerrainLightingCacheForTileChange(
    const State& state,
    Graphics& graphics,
    const IVec2& tile_pos
) {
    EnsureTerrainLightingCacheShape(state, graphics);
    if (!graphics.terrain_lighting_cache.valid) {
        RebuildTerrainLightingCache(state, graphics);
        return;
    }

    const int min_x = std::max(0, tile_pos.x - kTileChangeUpdateRadius);
    const int min_y = std::max(0, tile_pos.y - kTileChangeUpdateRadius);
    const int max_x = std::min(
        static_cast<int>(state.stage.GetTileWidth()) - 1,
        tile_pos.x + kTileChangeUpdateRadius
    );
    const int max_y = std::min(
        static_cast<int>(state.stage.GetTileHeight()) - 1,
        tile_pos.y + kTileChangeUpdateRadius
    );

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            graphics.terrain_lighting_cache.tiles[static_cast<std::size_t>(y)]
                                               [static_cast<std::size_t>(x)] =
                BuildTerrainLightingTile(state, x, y);
            graphics.backwall_lighting_cache.tiles[static_cast<std::size_t>(y)]
                                                 [static_cast<std::size_t>(x)] =
                BuildBackwallLightingTile(state, x, y);
        }
    }
}

TerrainLightingTile GetTerrainLightingTileForRender(
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
) {
    if (tile_x >= 0 && tile_y >= 0 &&
        tile_x < static_cast<int>(graphics.terrain_lighting_cache.tiles.empty()
                                      ? 0
                                      : graphics.terrain_lighting_cache.tiles.front().size()) &&
        tile_y < static_cast<int>(graphics.terrain_lighting_cache.tiles.size()) &&
        graphics.terrain_lighting_cache.valid) {
        return graphics.terrain_lighting_cache.tiles[static_cast<std::size_t>(tile_y)]
                                                  [static_cast<std::size_t>(tile_x)];
    }

    return BuildTerrainLightingTile(state, tile_x, tile_y);
}

BackwallLightingTile GetBackwallLightingTileForRender(
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
) {
    if (tile_x >= 0 && tile_y >= 0 &&
        tile_x < static_cast<int>(graphics.backwall_lighting_cache.tiles.empty()
                                      ? 0
                                      : graphics.backwall_lighting_cache.tiles.front().size()) &&
        tile_y < static_cast<int>(graphics.backwall_lighting_cache.tiles.size()) &&
        graphics.backwall_lighting_cache.valid) {
        return graphics.backwall_lighting_cache.tiles[static_cast<std::size_t>(tile_y)]
                                                   [static_cast<std::size_t>(tile_x)];
    }

    return BuildBackwallLightingTile(state, tile_x, tile_y);
}

} // namespace splonks

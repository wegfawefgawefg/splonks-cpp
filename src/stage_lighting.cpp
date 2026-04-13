#include "stage_lighting.hpp"

#include "state.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"

#include <algorithm>

namespace splonks {

namespace {

constexpr int kTileChangeUpdateRadius = 2;

bool IsForegroundSolidTile(Tile tile) {
    return GetTileArchetype(tile).solid;
}

bool StageTileExists(const State& state, int tile_x, int tile_y) {
    return tile_x >= 0 && tile_y >= 0 &&
           tile_x < static_cast<int>(state.stage.GetTileWidth()) &&
           tile_y < static_cast<int>(state.stage.GetTileHeight());
}

Tile GetTileForLighting(const State& state, int tile_x, int tile_y) {
    if (!StageTileExists(state, tile_x, tile_y)) {
        return state.settings.post_process.terrain_face_enclosed_stage_bounds
                   ? state.stage.GetTileOrBorder(tile_x, tile_y)
                   : Tile::Air;
    }

    return state.stage.tiles[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)];
}

bool IsBackwallLightingTile(const State& state, int tile_x, int tile_y) {
    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    return !IsForegroundSolidTile(tile);
}

float GetRawForegroundOpenness(const State& state, int tile_x, int tile_y) {
    const Tile center_tile = GetTileForLighting(state, tile_x, tile_y);
    if (!IsForegroundSolidTile(center_tile)) {
        return 0.5F;
    }

    const float diagonal_weight = std::clamp(
        state.settings.post_process.terrain_exposure_diagonal_weight,
        0.0F,
        1.0F
    );

    float openness = 0.0F;
    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y - 1))) {
        openness += 1.0F;
    }
    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y + 1))) {
        openness += 1.0F;
    }
    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x - 1, tile_y))) {
        openness += 1.0F;
    }
    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x + 1, tile_y))) {
        openness += 1.0F;
    }

    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x - 1, tile_y - 1))) {
        openness += diagonal_weight;
    }
    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x + 1, tile_y - 1))) {
        openness += diagonal_weight;
    }
    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x - 1, tile_y + 1))) {
        openness += diagonal_weight;
    }
    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x + 1, tile_y + 1))) {
        openness += diagonal_weight;
    }

    const float max_openness = 4.0F + (4.0F * diagonal_weight);
    return std::clamp(openness / max_openness, 0.0F, 1.0F);
}

float GetSmoothedForegroundOpenness(const State& state, int tile_x, int tile_y) {
    const float raw_openness = GetRawForegroundOpenness(state, tile_x, tile_y);
    const float smoothing = std::clamp(state.settings.post_process.terrain_exposure_smoothing, 0.0F, 1.0F);
    if (smoothing <= 0.0F) {
        return raw_openness;
    }

    float openness_sum = 0.0F;
    int samples = 0;
    for (int sample_y = tile_y - 1; sample_y <= tile_y + 1; ++sample_y) {
        for (int sample_x = tile_x - 1; sample_x <= tile_x + 1; ++sample_x) {
            if (!IsForegroundSolidTile(GetTileForLighting(state, sample_x, sample_y))) {
                continue;
            }
            openness_sum += GetRawForegroundOpenness(state, sample_x, sample_y);
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

float ComputeForegroundBrightness(const State& state, int tile_x, int tile_y) {
    if (!state.settings.post_process.terrain_exposure_lighting) {
        return 1.0F;
    }

    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y))) {
        return 1.0F;
    }

    const float openness = GetSmoothedForegroundOpenness(state, tile_x, tile_y);
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

ForegroundTileTopology BuildForegroundTileTopology(const State& state, int tile_x, int tile_y) {
    ForegroundTileTopology result;

    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    if (!IsForegroundSolidTile(tile)) {
        return result;
    }

    result.open_top = !IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y - 1));
    result.open_bottom = !IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y + 1));
    result.open_left = !IsForegroundSolidTile(GetTileForLighting(state, tile_x - 1, tile_y));
    result.open_right = !IsForegroundSolidTile(GetTileForLighting(state, tile_x + 1, tile_y));

    const bool open_top_left =
        !IsForegroundSolidTile(GetTileForLighting(state, tile_x - 1, tile_y - 1));
    const bool open_top_right =
        !IsForegroundSolidTile(GetTileForLighting(state, tile_x + 1, tile_y - 1));
    const bool open_bottom_left =
        !IsForegroundSolidTile(GetTileForLighting(state, tile_x - 1, tile_y + 1));
    const bool open_bottom_right =
        !IsForegroundSolidTile(GetTileForLighting(state, tile_x + 1, tile_y + 1));

    result.ao_top_left = !result.open_top && !result.open_left && open_top_left;
    result.ao_top_right = !result.open_top && !result.open_right && open_top_right;
    result.ao_bottom_left = !result.open_bottom && !result.open_left && open_bottom_left;
    result.ao_bottom_right = !result.open_bottom && !result.open_right && open_bottom_right;

    return result;
}

void EnsureStageLightingCacheShape(State& state) {
    const std::size_t tile_height = state.stage.tiles.size();
    const std::size_t tile_width =
        tile_height == 0 ? 0 : state.stage.tiles.front().size();
    const bool foreground_topology_shape_matches =
        state.stage_lighting.foreground_topology.tiles.size() == tile_height &&
        (tile_height == 0 ||
         state.stage_lighting.foreground_topology.tiles.front().size() == tile_width);
    const bool foreground_brightness_shape_matches =
        state.stage_lighting.foreground_brightness.tiles.size() == tile_height &&
        (tile_height == 0 ||
         state.stage_lighting.foreground_brightness.tiles.front().size() == tile_width);
    const bool backwall_brightness_shape_matches =
        state.stage_lighting.backwall_brightness.tiles.size() == tile_height &&
        (tile_height == 0 ||
         state.stage_lighting.backwall_brightness.tiles.front().size() == tile_width);
    if (foreground_topology_shape_matches &&
        foreground_brightness_shape_matches &&
        backwall_brightness_shape_matches) {
        return;
    }

    state.stage_lighting.foreground_topology.tiles.assign(
        tile_height,
        std::vector<ForegroundTileTopology>(tile_width)
    );
    state.stage_lighting.foreground_brightness.tiles.assign(
        tile_height,
        std::vector<float>(tile_width, 1.0F)
    );
    state.stage_lighting.backwall_brightness.tiles.assign(
        tile_height,
        std::vector<float>(tile_width, 1.0F)
    );
    state.stage_lighting.foreground_topology.valid = false;
    state.stage_lighting.foreground_brightness.valid = false;
    state.stage_lighting.backwall_brightness.valid = false;
}

} // namespace

ForegroundTopologyCache ForegroundTopologyCache::New() {
    return ForegroundTopologyCache{};
}

ForegroundBrightnessCache ForegroundBrightnessCache::New() {
    return ForegroundBrightnessCache{};
}

BackwallBrightnessCache BackwallBrightnessCache::New() {
    return BackwallBrightnessCache{};
}

StageLighting StageLighting::New() {
    StageLighting lighting;
    lighting.foreground_topology = ForegroundTopologyCache::New();
    lighting.foreground_brightness = ForegroundBrightnessCache::New();
    lighting.backwall_brightness = BackwallBrightnessCache::New();
    return lighting;
}

void InvalidateStageLighting(State& state) {
    state.stage_lighting.foreground_topology.valid = false;
    state.stage_lighting.foreground_brightness.valid = false;
    state.stage_lighting.backwall_brightness.valid = false;
}

void RebuildStageLighting(State& state) {
    EnsureStageLightingCacheShape(state);

    for (std::size_t y = 0; y < state.stage_lighting.foreground_topology.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage_lighting.foreground_topology.tiles[y].size(); ++x) {
            state.stage_lighting.foreground_topology.tiles[y][x] = BuildForegroundTileTopology(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            state.stage_lighting.foreground_brightness.tiles[y][x] = ComputeForegroundBrightness(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            state.stage_lighting.backwall_brightness.tiles[y][x] = ComputeBackwallBrightness(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
        }
    }

    state.stage_lighting.foreground_topology.valid = true;
    state.stage_lighting.foreground_brightness.valid = true;
    state.stage_lighting.backwall_brightness.valid = true;
}

void EnsureStageLighting(State& state) {
    EnsureStageLightingCacheShape(state);
    if (!state.stage_lighting.foreground_topology.valid ||
        !state.stage_lighting.foreground_brightness.valid ||
        !state.stage_lighting.backwall_brightness.valid) {
        RebuildStageLighting(state);
    }
}

void UpdateStageLightingForTileChange(State& state, const IVec2& tile_pos) {
    EnsureStageLightingCacheShape(state);
    if (!state.stage_lighting.foreground_topology.valid ||
        !state.stage_lighting.foreground_brightness.valid ||
        !state.stage_lighting.backwall_brightness.valid) {
        RebuildStageLighting(state);
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
            state.stage_lighting.foreground_topology.tiles[static_cast<std::size_t>(y)]
                                                        [static_cast<std::size_t>(x)] =
                BuildForegroundTileTopology(state, x, y);
            state.stage_lighting.foreground_brightness.tiles[static_cast<std::size_t>(y)]
                                                          [static_cast<std::size_t>(x)] =
                ComputeForegroundBrightness(state, x, y);
            state.stage_lighting.backwall_brightness.tiles[static_cast<std::size_t>(y)]
                                                        [static_cast<std::size_t>(x)] =
                ComputeBackwallBrightness(state, x, y);
        }
    }
}

ForegroundTileTopology GetForegroundTileTopologyForRender(const State& state, int tile_x, int tile_y) {
    if (tile_x >= 0 && tile_y >= 0 &&
        tile_x < static_cast<int>(state.stage_lighting.foreground_topology.tiles.empty()
                                      ? 0
                                      : state.stage_lighting.foreground_topology.tiles.front().size()) &&
        tile_y < static_cast<int>(state.stage_lighting.foreground_topology.tiles.size()) &&
        state.stage_lighting.foreground_topology.valid) {
        return state.stage_lighting.foreground_topology.tiles[static_cast<std::size_t>(tile_y)]
                                                           [static_cast<std::size_t>(tile_x)];
    }

    return BuildForegroundTileTopology(state, tile_x, tile_y);
}

float GetForegroundBrightnessForRender(const State& state, int tile_x, int tile_y) {
    if (tile_x >= 0 && tile_y >= 0 &&
        tile_x < static_cast<int>(state.stage_lighting.foreground_brightness.tiles.empty()
                                      ? 0
                                      : state.stage_lighting.foreground_brightness.tiles.front().size()) &&
        tile_y < static_cast<int>(state.stage_lighting.foreground_brightness.tiles.size()) &&
        state.stage_lighting.foreground_brightness.valid) {
        return state.stage_lighting.foreground_brightness.tiles[static_cast<std::size_t>(tile_y)]
                                                           [static_cast<std::size_t>(tile_x)];
    }

    return ComputeForegroundBrightness(state, tile_x, tile_y);
}


float GetBackwallBrightnessForRender(const State& state, int tile_x, int tile_y) {
    if (tile_x >= 0 && tile_y >= 0 &&
        tile_x < static_cast<int>(state.stage_lighting.backwall_brightness.tiles.empty()
                                      ? 0
                                      : state.stage_lighting.backwall_brightness.tiles.front().size()) &&
        tile_y < static_cast<int>(state.stage_lighting.backwall_brightness.tiles.size()) &&
        state.stage_lighting.backwall_brightness.valid) {
        return state.stage_lighting.backwall_brightness.tiles[static_cast<std::size_t>(tile_y)]
                                                        [static_cast<std::size_t>(tile_x)];
    }

    return ComputeBackwallBrightness(state, tile_x, tile_y);
}

} // namespace splonks

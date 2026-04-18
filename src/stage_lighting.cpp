#include "stage_lighting.hpp"

#include "state.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace splonks {

namespace {

constexpr int kTileChangeUpdateRadius = 2;
constexpr float kStageLightBrightnessBoost = 1.0F;

float ApplySignalRemap(float value, bool enabled, float input_min, float input_max, float gamma) {
    const float clamped_value = std::clamp(value, 0.0F, 1.0F);
    if (!enabled) {
        return clamped_value;
    }

    const float low = std::clamp(std::min(input_min, input_max), 0.0F, 1.0F);
    const float high = std::clamp(std::max(input_min, input_max), 0.0F, 1.0F);
    if (high - low <= 0.0001F) {
        return clamped_value >= high ? 1.0F : 0.0F;
    }

    const float normalized = std::clamp((clamped_value - low) / (high - low), 0.0F, 1.0F);
    return std::pow(normalized, std::max(gamma, 0.01F));
}

float ApplyOutputLevels(float value, bool enabled, float min_value, float max_value) {
    const float clamped_value = std::clamp(value, 0.0F, 1.0F);
    if (!enabled) {
        return clamped_value;
    }

    const float low = std::min(min_value, max_value);
    const float high = std::max(min_value, max_value);
    return std::lerp(low, high, clamped_value);
}

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

int GetWrappedTileDelta(int from, int to, int size) {
    int delta = to - from;
    if (size <= 0) {
        return delta;
    }
    const int positive = delta + size;
    const int negative = delta - size;
    if (std::abs(positive) < std::abs(delta)) {
        delta = positive;
    }
    if (std::abs(negative) < std::abs(delta)) {
        delta = negative;
    }
    return delta;
}

bool IsTileWithinStageLightRadius(const State& state, const StageLight& light, int tile_x, int tile_y) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    const int dx = state.stage.WrapsX()
                       ? GetWrappedTileDelta(tile_x, light.tile_pos.x, stage_width)
                       : light.tile_pos.x - tile_x;
    const int dy = state.stage.WrapsY()
                       ? GetWrappedTileDelta(tile_y, light.tile_pos.y, stage_height)
                       : light.tile_pos.y - tile_y;
    const float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
    return distance <= static_cast<float>(light.radius);
}

float GetStageLightFalloff(const State& state, const StageLight& light, int tile_x, int tile_y) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    const int dx = state.stage.WrapsX()
                       ? GetWrappedTileDelta(tile_x, light.tile_pos.x, stage_width)
                       : light.tile_pos.x - tile_x;
    const int dy = state.stage.WrapsY()
                       ? GetWrappedTileDelta(tile_y, light.tile_pos.y, stage_height)
                       : light.tile_pos.y - tile_y;
    const float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
    if (distance > static_cast<float>(light.radius)) {
        return 0.0F;
    }
    return 1.0F - (distance / static_cast<float>(light.radius));
}

void ApplyStageLightContribution(
    State& state,
    const StageLight& light,
    const std::vector<std::vector<bool>>* allowed_tiles = nullptr
) {
    if (light.radius <= 0) {
        return;
    }

    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return;
    }

    std::vector<std::vector<bool>> visited(
        static_cast<std::size_t>(stage_height),
        std::vector<bool>(static_cast<std::size_t>(stage_width), false)
    );
    std::deque<IVec2> open;
    open.push_back(state.stage.WrapTileCoord(light.tile_pos));

    while (!open.empty()) {
        const IVec2 current = open.front();
        open.pop_front();

        if (!state.stage.IsTileCoordInside(current.x, current.y)) {
            continue;
        }

        if (visited[static_cast<std::size_t>(current.y)][static_cast<std::size_t>(current.x)]) {
            continue;
        }
        visited[static_cast<std::size_t>(current.y)][static_cast<std::size_t>(current.x)] = true;

        if (!IsTileWithinStageLightRadius(state, light, current.x, current.y)) {
            continue;
        }
        if (!IsBackwallLightingTile(state, current.x, current.y)) {
            continue;
        }

        if (allowed_tiles == nullptr ||
            (*allowed_tiles)[static_cast<std::size_t>(current.y)][static_cast<std::size_t>(current.x)]) {
            const float falloff = GetStageLightFalloff(state, light, current.x, current.y);
            float& brightness =
                state.stage_lighting.backwall_light_brightness.tiles[static_cast<std::size_t>(current.y)]
                                                              [static_cast<std::size_t>(current.x)];
            brightness = std::clamp(
                brightness + (kStageLightBrightnessBoost * falloff),
                0.0F,
                2.0F
            );
        }

        const IVec2 neighbors[] = {
            IVec2::New(current.x - 1, current.y),
            IVec2::New(current.x + 1, current.y),
            IVec2::New(current.x, current.y - 1),
            IVec2::New(current.x, current.y + 1),
        };
        for (const IVec2& neighbor_unwrapped : neighbors) {
            const IVec2 neighbor = state.stage.WrapTileCoord(neighbor_unwrapped);
            if (!state.stage.IsTileCoordInside(neighbor.x, neighbor.y)) {
                continue;
            }
            if (!IsTileWithinStageLightRadius(state, light, neighbor.x, neighbor.y)) {
                continue;
            }
            open.push_back(neighbor);
        }
    }
}

void ApplyStageLightsToBackwallBrightness(State& state) {
    for (const StageLight& light : state.stage.lights) {
        ApplyStageLightContribution(state, light);
    }
}

bool IsTileAffectedByStageLight(const State& state, const StageLight& light, const IVec2& tile_pos) {
    return IsTileWithinStageLightRadius(state, light, tile_pos.x, tile_pos.y);
}

std::vector<const StageLight*> GetStageLightsAffectedByTileChange(const State& state, const IVec2& tile_pos) {
    std::vector<const StageLight*> affected_lights;
    affected_lights.reserve(state.stage.lights.size());
    for (const StageLight& light : state.stage.lights) {
        if (light.radius <= 0) {
            continue;
        }
        if (IsTileAffectedByStageLight(state, light, tile_pos)) {
            affected_lights.push_back(&light);
        }
    }
    return affected_lights;
}

std::vector<const StageLight*> GetStageLightsAffectedByTileChanges(
    const State& state,
    const std::vector<IVec2>& tile_positions
) {
    std::vector<const StageLight*> affected_lights;
    std::vector<bool> seen_lights(state.stage.lights.size(), false);

    for (const IVec2& tile_pos : tile_positions) {
        for (std::size_t light_idx = 0; light_idx < state.stage.lights.size(); ++light_idx) {
            if (seen_lights[light_idx]) {
                continue;
            }

            const StageLight& light = state.stage.lights[light_idx];
            if (light.radius <= 0) {
                continue;
            }
            if (!IsTileAffectedByStageLight(state, light, tile_pos)) {
                continue;
            }

            seen_lights[light_idx] = true;
            affected_lights.push_back(&light);
        }
    }

    return affected_lights;
}

void MarkTilesAffectedByStageLight(
    const State& state,
    const StageLight& light,
    std::vector<std::vector<bool>>& affected_tiles
) {
    const int radius = light.radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const IVec2 tile_pos = state.stage.WrapTileCoord(
                IVec2::New(light.tile_pos.x + dx, light.tile_pos.y + dy)
            );
            if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                continue;
            }
            if (!IsTileAffectedByStageLight(state, light, tile_pos)) {
                continue;
            }
            affected_tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)] = true;
        }
    }
}

bool DoesStageLightAffectAnyMarkedTile(
    const State& state,
    const StageLight& light,
    const std::vector<std::vector<bool>>& affected_tiles
) {
    const int radius = light.radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const IVec2 tile_pos = state.stage.WrapTileCoord(
                IVec2::New(light.tile_pos.x + dx, light.tile_pos.y + dy)
            );
            if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                continue;
            }
            if (!affected_tiles[static_cast<std::size_t>(tile_pos.y)][static_cast<std::size_t>(tile_pos.x)]) {
                continue;
            }
            if (!IsTileAffectedByStageLight(state, light, tile_pos)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

void RebuildAffectedStageLightRegions(State& state, const std::vector<IVec2>& tile_positions) {
    const std::vector<const StageLight*> first_pass_lights =
        GetStageLightsAffectedByTileChanges(state, tile_positions);
    if (first_pass_lights.empty()) {
        return;
    }

    const std::size_t tile_height = state.stage_lighting.backwall_light_brightness.tiles.size();
    const std::size_t tile_width =
        tile_height == 0 ? 0 : state.stage_lighting.backwall_light_brightness.tiles.front().size();
    std::vector<std::vector<bool>> affected_tiles(
        tile_height,
        std::vector<bool>(tile_width, false)
    );

    for (const StageLight* light : first_pass_lights) {
        MarkTilesAffectedByStageLight(state, *light, affected_tiles);
    }

    std::vector<const StageLight*> closed_lights;
    closed_lights.reserve(state.stage.lights.size());
    for (const StageLight& light : state.stage.lights) {
        if (light.radius <= 0) {
            continue;
        }
        if (!DoesStageLightAffectAnyMarkedTile(state, light, affected_tiles)) {
            continue;
        }
        closed_lights.push_back(&light);
    }

    for (std::size_t y = 0; y < affected_tiles.size(); ++y) {
        for (std::size_t x = 0; x < affected_tiles[y].size(); ++x) {
            if (!affected_tiles[y][x]) {
                continue;
            }
            state.stage_lighting.backwall_light_brightness.tiles[y][x] = 0.0F;
        }
    }

    for (const StageLight* light : closed_lights) {
        ApplyStageLightContribution(state, *light, &affected_tiles);
    }
}

float ComputeForegroundBrightness(const State& state, int tile_x, int tile_y) {
    if (!state.settings.post_process.terrain_exposure_lighting) {
        return 1.0F;
    }

    if (!IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y))) {
        return 1.0F;
    }

    const float openness = GetSmoothedForegroundOpenness(state, tile_x, tile_y);
    const float remapped_openness = ApplySignalRemap(
        openness,
        state.settings.post_process.terrain_exposure_remap_enabled,
        state.settings.post_process.terrain_exposure_input_min,
        state.settings.post_process.terrain_exposure_input_max,
        state.settings.post_process.terrain_exposure_gamma
    );
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
    const float target_brightness = ApplyOutputLevels(
        remapped_openness,
        state.settings.post_process.terrain_exposure_output_levels_enabled,
        min_brightness,
        max_brightness
    );
    return std::clamp(std::lerp(1.0F, target_brightness, amount), 0.0F, 2.0F);
}

float ComputeBackwallBrightness(const State& state, int tile_x, int tile_y) {
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.backwall_lighting) {
        return 1.0F;
    }

    // Covered backwall should stay dim so foreground shake reveals wall,
    // not an incorrectly lit gap behind it.
    if (IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y))) {
        return 0.4F;
    }

    const float openness = GetSmoothedBackwallOpenness(state, tile_x, tile_y);
    const float remapped_openness = ApplySignalRemap(
        openness,
        state.settings.post_process.backwall_remap_enabled,
        state.settings.post_process.backwall_input_min,
        state.settings.post_process.backwall_input_max,
        state.settings.post_process.backwall_gamma
    );
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
    const float brightness_scale = std::clamp(
        state.settings.post_process.backwall_brightness,
        0.0F,
        2.0F
    );
    const float base_brightness = ApplyOutputLevels(
        remapped_openness,
        state.settings.post_process.backwall_output_levels_enabled,
        min_brightness,
        max_brightness
    );
    return std::clamp(base_brightness * brightness_scale, 0.0F, 2.0F);
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
    const bool backwall_base_brightness_shape_matches =
        state.stage_lighting.backwall_base_brightness.tiles.size() == tile_height &&
        (tile_height == 0 ||
         state.stage_lighting.backwall_base_brightness.tiles.front().size() == tile_width);
    const bool backwall_light_brightness_shape_matches =
        state.stage_lighting.backwall_light_brightness.tiles.size() == tile_height &&
        (tile_height == 0 ||
         state.stage_lighting.backwall_light_brightness.tiles.front().size() == tile_width);
    if (foreground_topology_shape_matches &&
        foreground_brightness_shape_matches &&
        backwall_base_brightness_shape_matches &&
        backwall_light_brightness_shape_matches) {
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
    state.stage_lighting.backwall_base_brightness.tiles.assign(
        tile_height,
        std::vector<float>(tile_width, 1.0F)
    );
    state.stage_lighting.backwall_light_brightness.tiles.assign(
        tile_height,
        std::vector<float>(tile_width, 0.0F)
    );
    state.stage_lighting.foreground_topology.valid = false;
    state.stage_lighting.foreground_brightness.valid = false;
    state.stage_lighting.backwall_base_brightness.valid = false;
    state.stage_lighting.backwall_light_brightness.valid = false;
}

} // namespace

VID AddStageLight(State& state, const IVec2& tile_pos, int radius) {
    const VID vid = state.stage.AddLight(tile_pos, radius);
    InvalidateStageLighting(state);
    return vid;
}

bool RemoveStageLight(State& state, VID vid) {
    const bool removed = state.stage.RemoveLight(vid);
    if (removed) {
        InvalidateStageLighting(state);
    }
    return removed;
}

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
    lighting.backwall_base_brightness = BackwallBrightnessCache::New();
    lighting.backwall_light_brightness = BackwallBrightnessCache::New();
    return lighting;
}

void InvalidateStageLighting(State& state) {
    state.stage_lighting.foreground_topology.valid = false;
    state.stage_lighting.foreground_brightness.valid = false;
    state.stage_lighting.backwall_base_brightness.valid = false;
    state.stage_lighting.backwall_light_brightness.valid = false;
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
            state.stage_lighting.backwall_base_brightness.tiles[y][x] = ComputeBackwallBrightness(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            state.stage_lighting.backwall_light_brightness.tiles[y][x] = 0.0F;
        }
    }

    ApplyStageLightsToBackwallBrightness(state);

    state.stage_lighting.foreground_topology.valid = true;
    state.stage_lighting.foreground_brightness.valid = true;
    state.stage_lighting.backwall_base_brightness.valid = true;
    state.stage_lighting.backwall_light_brightness.valid = true;
}

void EnsureStageLighting(State& state) {
    EnsureStageLightingCacheShape(state);
    if (!state.stage_lighting.foreground_topology.valid ||
        !state.stage_lighting.foreground_brightness.valid ||
        !state.stage_lighting.backwall_base_brightness.valid ||
        !state.stage_lighting.backwall_light_brightness.valid) {
        RebuildStageLighting(state);
    }
}

void UpdateStageLightingForTileChange(State& state, const IVec2& tile_pos) {
    const std::vector<IVec2> tile_positions{tile_pos};
    UpdateStageLightingForTileChanges(state, tile_positions);
}

void UpdateStageLightingForTileChanges(State& state, const std::vector<IVec2>& tile_positions) {
    if (tile_positions.empty()) {
        return;
    }

    EnsureStageLightingCacheShape(state);
    if (!state.stage_lighting.foreground_topology.valid ||
        !state.stage_lighting.foreground_brightness.valid ||
        !state.stage_lighting.backwall_base_brightness.valid ||
        !state.stage_lighting.backwall_light_brightness.valid) {
        RebuildStageLighting(state);
        return;
    }

    int min_x = static_cast<int>(state.stage.GetTileWidth()) - 1;
    int min_y = static_cast<int>(state.stage.GetTileHeight()) - 1;
    int max_x = 0;
    int max_y = 0;
    for (const IVec2& tile_pos : tile_positions) {
        min_x = std::min(min_x, std::max(0, tile_pos.x - kTileChangeUpdateRadius));
        min_y = std::min(min_y, std::max(0, tile_pos.y - kTileChangeUpdateRadius));
        max_x = std::max(
            max_x,
            std::min(static_cast<int>(state.stage.GetTileWidth()) - 1, tile_pos.x + kTileChangeUpdateRadius)
        );
        max_y = std::max(
            max_y,
            std::min(static_cast<int>(state.stage.GetTileHeight()) - 1, tile_pos.y + kTileChangeUpdateRadius)
        );
    }

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            state.stage_lighting.foreground_topology.tiles[static_cast<std::size_t>(y)]
                                                        [static_cast<std::size_t>(x)] =
                BuildForegroundTileTopology(state, x, y);
            state.stage_lighting.foreground_brightness.tiles[static_cast<std::size_t>(y)]
                                                          [static_cast<std::size_t>(x)] =
                ComputeForegroundBrightness(state, x, y);
            state.stage_lighting.backwall_base_brightness.tiles[static_cast<std::size_t>(y)]
                                                             [static_cast<std::size_t>(x)] =
                ComputeBackwallBrightness(state, x, y);
        }
    }

    RebuildAffectedStageLightRegions(state, tile_positions);
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
        tile_x < static_cast<int>(state.stage_lighting.backwall_base_brightness.tiles.empty()
                                      ? 0
                                      : state.stage_lighting.backwall_base_brightness.tiles.front().size()) &&
        tile_y < static_cast<int>(state.stage_lighting.backwall_base_brightness.tiles.size()) &&
        state.stage_lighting.backwall_base_brightness.valid &&
        state.stage_lighting.backwall_light_brightness.valid) {
        return std::clamp(
            state.stage_lighting.backwall_base_brightness.tiles[static_cast<std::size_t>(tile_y)]
                                                           [static_cast<std::size_t>(tile_x)] +
                state.stage_lighting.backwall_light_brightness.tiles[static_cast<std::size_t>(tile_y)]
                                                                [static_cast<std::size_t>(tile_x)],
            0.0F,
            2.0F
        );
    }

    return ComputeBackwallBrightness(state, tile_x, tile_y);
}

} // namespace splonks

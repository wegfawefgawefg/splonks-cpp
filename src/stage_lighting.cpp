#include "stage_lighting.hpp"

#include "state.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace splonks {

namespace {

constexpr float kLiveAirAmbient = 0.10F;
constexpr float kLiveSolidAmbient = 0.06F;
constexpr float kLiveCoveredBackwallAmbient = 0.04F;
constexpr float kLiveStageLightSource = 1.55F;
constexpr float kLivePlayerLightSource = 1.45F;
constexpr int kLivePlayerLightRadius = 13;
constexpr float kLiveAirDecay = 0.075F;
constexpr float kLiveSolidDecay = 0.18F;
constexpr float kLiveFluidDecay = 0.11F;
constexpr int kLiveMinimumPropagationPasses = 18;

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
        return state.stage.GetTileOrBorder(tile_x, tile_y);
    }

    return state.stage.tiles[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)];
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

float GetLiveLightSeedForTile(const State& state, int tile_x, int tile_y) {
    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    return IsForegroundSolidTile(tile) ? kLiveSolidAmbient : kLiveAirAmbient;
}

float GetLiveLightDecayIntoTile(const State& state, int tile_x, int tile_y) {
    if (StageTileExists(state, tile_x, tile_y)) {
        const float fluid_amount = state.stage.GetFluidAmount(
            static_cast<unsigned int>(tile_x),
            static_cast<unsigned int>(tile_y)
        );
        if (fluid_amount > 0.001F) {
            return kLiveFluidDecay;
        }
    }

    return IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y))
               ? kLiveSolidDecay
               : kLiveAirDecay;
}

void ApplyLiveLightSourceAtTile(
    const State& state,
    std::vector<std::vector<float>>& light_grid,
    const IVec2& raw_center,
    int radius,
    float source
) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0 || radius <= 0 || source <= 0.0F) {
        return;
    }

    const IVec2 center = state.stage.WrapTileCoord(raw_center);
    if (!state.stage.IsTileCoordInside(center.x, center.y)) {
        return;
    }

    // Seed only the emitter cell. Spread and occlusion are handled by propagation,
    // so sources do not paint light through nearby solid walls.
    float& light_value =
        light_grid[static_cast<std::size_t>(center.y)]
                  [static_cast<std::size_t>(center.x)];
    light_value = std::max(light_value, source);
}

void ApplyLiveLightSourceAtWorldPos(
    const State& state,
    std::vector<std::vector<float>>& light_grid,
    const Vec2& world_pos,
    float source
) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0 || source <= 0.0F) {
        return;
    }

    const float tile_x = (world_pos.x / static_cast<float>(kTileSize)) - 0.5F;
    const float tile_y = (world_pos.y / static_cast<float>(kTileSize)) - 0.5F;
    const int base_x = static_cast<int>(std::floor(tile_x));
    const int base_y = static_cast<int>(std::floor(tile_y));

    for (int offset_y = 0; offset_y <= 1; ++offset_y) {
        for (int offset_x = 0; offset_x <= 1; ++offset_x) {
            const IVec2 tile_pos = state.stage.WrapTileCoord(
                IVec2::New(base_x + offset_x, base_y + offset_y)
            );
            if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                continue;
            }

            const Vec2 tile_center = Vec2::New(
                (static_cast<float>(base_x + offset_x) + 0.5F) * static_cast<float>(kTileSize),
                (static_cast<float>(base_y + offset_y) + 0.5F) * static_cast<float>(kTileSize)
            );
            const float distance_tiles =
                Length(tile_center - world_pos) / static_cast<float>(kTileSize);
            const float tile_center_source = std::max(
                0.0F,
                source - (distance_tiles * kLiveAirDecay)
            );
            float& light_value =
                light_grid[static_cast<std::size_t>(tile_pos.y)]
                          [static_cast<std::size_t>(tile_pos.x)];
            light_value = std::max(light_value, tile_center_source);
        }
    }
}

void ApplyLiveStageLightSources(State& state, std::vector<std::vector<float>>& light_grid) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return;
    }

    for (const StageLight& light : state.stage.lights) {
        if (light.radius <= 0) {
            continue;
        }
        const IVec2 center = state.stage.WrapTileCoord(light.tile_pos);
        if (!state.stage.IsTileCoordInside(center.x, center.y)) {
            continue;
        }

        const float source = kLiveStageLightSource + (static_cast<float>(light.radius) * 0.03F);
        ApplyLiveLightSourceAtTile(state, light_grid, center, light.radius, source);
    }
}

void ApplyLiveControlledEntityLightSource(State& state, std::vector<std::vector<float>>& light_grid) {
    const Entity* entity = nullptr;
    if (state.controlled_entity_vid.has_value()) {
        entity = state.entity_manager.GetEntity(*state.controlled_entity_vid);
    }
    if ((entity == nullptr || !entity->active || entity->condition == EntityCondition::Dead) &&
        state.player_vid.has_value()) {
        entity = state.entity_manager.GetEntity(*state.player_vid);
    }
    if (entity == nullptr || !entity->active || entity->condition == EntityCondition::Dead) {
        return;
    }

    const Vec2 world_pos = entity->GetCenter();
    ApplyLiveLightSourceAtWorldPos(
        state,
        light_grid,
        world_pos,
        kLivePlayerLightSource * std::max(state.settings.post_process.player_lamp_strength, 0.0F)
    );
}

void PropagateLiveStageLighting(State& state, std::vector<std::vector<float>>& light_grid) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return;
    }

    int max_light_radius = 0;
    for (const StageLight& light : state.stage.lights) {
        max_light_radius = std::max(max_light_radius, light.radius);
    }
    if (state.controlled_entity_vid.has_value() || state.player_vid.has_value()) {
        max_light_radius = std::max(max_light_radius, kLivePlayerLightRadius);
    }
    const int pass_cap = std::max(kLiveMinimumPropagationPasses, stage_width + stage_height);
    const int passes = std::clamp(
        std::max(kLiveMinimumPropagationPasses, max_light_radius + 4),
        kLiveMinimumPropagationPasses,
        pass_cap
    );

    std::vector<std::vector<float>> next_grid = light_grid;
    constexpr IVec2 kNeighbors[] = {
        IVec2{-1, 0},
        IVec2{1, 0},
        IVec2{0, -1},
        IVec2{0, 1},
    };

    for (int pass = 0; pass < passes; ++pass) {
        next_grid = light_grid;
        for (int y = 0; y < stage_height; ++y) {
            for (int x = 0; x < stage_width; ++x) {
                const float source = light_grid[static_cast<std::size_t>(y)]
                                               [static_cast<std::size_t>(x)];
                if (source <= 0.001F) {
                    continue;
                }
                if (IsForegroundSolidTile(GetTileForLighting(state, x, y))) {
                    continue;
                }

                for (const IVec2& neighbor_delta : kNeighbors) {
                    const IVec2 neighbor = state.stage.WrapTileCoord(
                        IVec2::New(x + neighbor_delta.x, y + neighbor_delta.y)
                    );
                    if (!state.stage.IsTileCoordInside(neighbor.x, neighbor.y)) {
                        continue;
                    }

                    const float candidate =
                        source - GetLiveLightDecayIntoTile(state, neighbor.x, neighbor.y);
                    if (candidate <= 0.0F) {
                        continue;
                    }
                    float& target =
                        next_grid[static_cast<std::size_t>(neighbor.y)]
                                 [static_cast<std::size_t>(neighbor.x)];
                    target = std::max(target, candidate);
                }
            }
        }
        light_grid.swap(next_grid);
    }
}

std::vector<std::vector<float>> BuildLiveStageLightGrid(State& state) {
    const std::size_t tile_height = state.stage.tiles.size();
    const std::size_t tile_width =
        tile_height == 0 ? 0 : state.stage.tiles.front().size();
    std::vector<std::vector<float>> light_grid(
        tile_height,
        std::vector<float>(tile_width, 0.0F)
    );

    for (std::size_t y = 0; y < tile_height; ++y) {
        for (std::size_t x = 0; x < tile_width; ++x) {
            light_grid[y][x] = GetLiveLightSeedForTile(state, static_cast<int>(x), static_cast<int>(y));
        }
    }

    ApplyLiveStageLightSources(state, light_grid);
    ApplyLiveControlledEntityLightSource(state, light_grid);
    PropagateLiveStageLighting(state, light_grid);
    return light_grid;
}

float ApplyForegroundLightingSettings(const State& state, float light_value) {
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.terrain_exposure_lighting) {
        return 1.0F;
    }

    const float remapped_light = ApplySignalRemap(
        light_value,
        state.settings.post_process.terrain_exposure_remap_enabled,
        state.settings.post_process.terrain_exposure_input_min,
        state.settings.post_process.terrain_exposure_input_max,
        state.settings.post_process.terrain_exposure_gamma
    );
    const float leveled_light = ApplyOutputLevels(
        remapped_light,
        state.settings.post_process.terrain_exposure_output_levels_enabled,
        state.settings.post_process.terrain_exposure_min_brightness,
        state.settings.post_process.terrain_exposure_max_brightness
    );
    const float amount = std::clamp(state.settings.post_process.terrain_exposure_amount, 0.0F, 1.0F);
    return std::clamp(std::lerp(1.0F, leveled_light, amount), 0.0F, 2.0F);
}

float ApplyBackwallLightingSettings(const State& state, float light_value, bool covered_by_solid) {
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.backwall_lighting) {
        return 1.0F;
    }

    const float covered_light = covered_by_solid
                                    ? std::min(light_value, kLiveCoveredBackwallAmbient)
                                    : light_value;
    const float remapped_light = ApplySignalRemap(
        covered_light,
        state.settings.post_process.backwall_remap_enabled,
        state.settings.post_process.backwall_input_min,
        state.settings.post_process.backwall_input_max,
        state.settings.post_process.backwall_gamma
    );
    const float leveled_light = ApplyOutputLevels(
        remapped_light,
        state.settings.post_process.backwall_output_levels_enabled,
        state.settings.post_process.backwall_min_brightness,
        state.settings.post_process.backwall_max_brightness
    );
    const float brightness_scale = std::clamp(
        state.settings.post_process.backwall_brightness,
        0.0F,
        2.0F
    );
    return std::clamp(leveled_light * brightness_scale, 0.0F, 2.0F);
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
    state.stage_lighting.rebuilt_stage_frame = std::numeric_limits<std::uint32_t>::max();
}

void RebuildStageLighting(State& state) {
    EnsureStageLightingCacheShape(state);

    const std::vector<std::vector<float>> live_light_grid = BuildLiveStageLightGrid(state);

    for (std::size_t y = 0; y < state.stage_lighting.foreground_topology.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage_lighting.foreground_topology.tiles[y].size(); ++x) {
            state.stage_lighting.foreground_topology.tiles[y][x] = BuildForegroundTileTopology(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            const float live_light = live_light_grid[y][x];
            state.stage_lighting.foreground_brightness.tiles[y][x] =
                ApplyForegroundLightingSettings(state, live_light);
            state.stage_lighting.backwall_base_brightness.tiles[y][x] = ApplyBackwallLightingSettings(
                state,
                live_light,
                IsForegroundSolidTile(GetTileForLighting(state, static_cast<int>(x), static_cast<int>(y)))
            );
            state.stage_lighting.backwall_light_brightness.tiles[y][x] = 0.0F;
        }
    }

    state.stage_lighting.foreground_topology.valid = true;
    state.stage_lighting.foreground_brightness.valid = true;
    state.stage_lighting.backwall_base_brightness.valid = true;
    state.stage_lighting.backwall_light_brightness.valid = true;
    state.stage_lighting.rebuilt_stage_frame = state.stage_frame;
}

void EnsureStageLighting(State& state) {
    EnsureStageLightingCacheShape(state);
    if (!state.stage_lighting.foreground_topology.valid ||
        !state.stage_lighting.foreground_brightness.valid ||
        !state.stage_lighting.backwall_base_brightness.valid ||
        !state.stage_lighting.backwall_light_brightness.valid ||
        state.stage_lighting.rebuilt_stage_frame != state.stage_frame) {
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
    InvalidateStageLighting(state);
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

    return ApplyForegroundLightingSettings(state, GetLiveLightSeedForTile(state, tile_x, tile_y));
}

float SampleBrightnessForRender(
    const State& state,
    const Vec2& world_pos,
    float (*sample_tile)(const State&, int, int)
) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return 1.0F;
    }

    const float tile_x = (world_pos.x / static_cast<float>(kTileSize)) - 0.5F;
    const float tile_y = (world_pos.y / static_cast<float>(kTileSize)) - 0.5F;
    const int base_x = static_cast<int>(std::floor(tile_x));
    const int base_y = static_cast<int>(std::floor(tile_y));
    const float frac_x = tile_x - static_cast<float>(base_x);
    const float frac_y = tile_y - static_cast<float>(base_y);

    float weighted_brightness = 0.0F;
    float total_weight = 0.0F;
    for (int offset_y = 0; offset_y <= 1; ++offset_y) {
        for (int offset_x = 0; offset_x <= 1; ++offset_x) {
            const float weight_x = offset_x == 0 ? 1.0F - frac_x : frac_x;
            const float weight_y = offset_y == 0 ? 1.0F - frac_y : frac_y;
            const float weight = weight_x * weight_y;
            if (weight <= 0.0F) {
                continue;
            }

            const IVec2 tile_pos = state.stage.WrapTileCoord(
                IVec2::New(base_x + offset_x, base_y + offset_y)
            );
            if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                continue;
            }

            weighted_brightness += sample_tile(state, tile_pos.x, tile_pos.y) * weight;
            total_weight += weight;
        }
    }

    if (total_weight <= 0.0F) {
        const IVec2 fallback_tile = state.stage.GetTileCoordAtWc(ToIVec2(world_pos));
        return sample_tile(state, fallback_tile.x, fallback_tile.y);
    }
    return weighted_brightness / total_weight;
}

float SampleForegroundBrightnessForRender(const State& state, const Vec2& world_pos) {
    return SampleBrightnessForRender(state, world_pos, GetForegroundBrightnessForRender);
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

    return ApplyBackwallLightingSettings(
        state,
        GetLiveLightSeedForTile(state, tile_x, tile_y),
        IsForegroundSolidTile(GetTileForLighting(state, tile_x, tile_y))
    );
}

float SampleBackwallBrightnessForRender(const State& state, const Vec2& world_pos) {
    return SampleBrightnessForRender(state, world_pos, GetBackwallBrightnessForRender);
}

} // namespace splonks

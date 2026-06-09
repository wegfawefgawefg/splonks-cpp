#include "stage_lighting.hpp"

#include "state.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"
#include "fxp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace splonks {

namespace {

constexpr float kLiveAirAmbient = 0.10F;
constexpr float kLiveSolidAmbient = 0.06F;
constexpr float kLiveCoveredBackwallAmbient = 0.04F;
constexpr float kLiveStageLightSource = 1.55F;
constexpr float kLiveAirDecay = 0.075F;
constexpr float kLiveSolidDecay = 0.18F;
constexpr float kLiveFluidDecay = 0.11F;
constexpr int kLiveMinimumPropagationPasses = 18;

struct LiveLightSource {
    FVec2 world_pos = FVec2::New(0.0F, 0.0F);
    int radius = 0;
    float source = 0.0F;
    Color3 color = Color3::White();
};

float GetBrightnessFromColor(Color3 color) {
    return (color.r + color.g + color.b) / 3.0F;
}

Color3 ClampColor(Color3 color, float min_value = 0.0F, float max_value = 2.0F) {
    return Color3::New(
        std::clamp(color.r, min_value, max_value),
        std::clamp(color.g, min_value, max_value),
        std::clamp(color.b, min_value, max_value)
    );
}

Color3 MaxColor(Color3 left, Color3 right) {
    return Color3::New(
        std::max(left.r, right.r),
        std::max(left.g, right.g),
        std::max(left.b, right.b)
    );
}

Color3 LerpColor(Color3 left, Color3 right, float amount) {
    return Color3::New(
        std::lerp(left.r, right.r, amount),
        std::lerp(left.g, right.g, amount),
        std::lerp(left.b, right.b, amount)
    );
}

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
    return GetTileSpec(tile).solid;
}

bool StageTileExists(const State& state, int tile_x, int tile_y) {
    return tile_x >= 0 && tile_y >= 0 &&
           tile_x < static_cast<int>(state.stage.GetTileWidth()) &&
           tile_y < static_cast<int>(state.stage.GetTileHeight());
}

std::optional<IVec2> GetCachedLightingTilePosForRender(const State& state, int tile_x, int tile_y) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return std::nullopt;
    }
    if (StageTileExists(state, tile_x, tile_y)) {
        return IVec2::New(tile_x, tile_y);
    }
    const IVec2 wrapped = state.stage.WrapTileCoord(IVec2::New(tile_x, tile_y));
    if (state.stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
        return wrapped;
    }
    return IVec2::New(
        std::clamp(tile_x, 0, stage_width - 1),
        std::clamp(tile_y, 0, stage_height - 1)
    );
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
        std::vector<Color3>(tile_width, Color3::White())
    );
    state.stage_lighting.backwall_base_brightness.tiles.assign(
        tile_height,
        std::vector<Color3>(tile_width, Color3::White())
    );
    state.stage_lighting.backwall_light_brightness.tiles.assign(
        tile_height,
        std::vector<Color3>(tile_width, Color3::White(0.0F))
    );
    state.stage_lighting.foreground_topology.valid = false;
    state.stage_lighting.foreground_brightness.valid = false;
    state.stage_lighting.backwall_base_brightness.valid = false;
    state.stage_lighting.backwall_light_brightness.valid = false;
}

Color3 GetLiveLightSeedForTile(const State& state, int tile_x, int tile_y) {
    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    const float base_ambient = IsForegroundSolidTile(tile) ? kLiveSolidAmbient : kLiveAirAmbient;
    const float openness_strength = std::clamp(
        state.settings.post_process.openness_ambient_strength,
        0.0F,
        1.0F
    );
    if (openness_strength <= 0.0F) {
        return Color3::White(base_ambient);
    }

    const float diagonal_weight = std::clamp(
        state.settings.post_process.terrain_exposure_diagonal_weight,
        0.0F,
        1.0F
    );
    const auto is_open = [&state](int x, int y) {
        return !IsForegroundSolidTile(GetTileForLighting(state, x, y));
    };

    float openness = 0.0F;
    openness += is_open(tile_x, tile_y - 1) ? 1.0F : 0.0F;
    openness += is_open(tile_x, tile_y + 1) ? 1.0F : 0.0F;
    openness += is_open(tile_x - 1, tile_y) ? 1.0F : 0.0F;
    openness += is_open(tile_x + 1, tile_y) ? 1.0F : 0.0F;
    openness += is_open(tile_x - 1, tile_y - 1) ? diagonal_weight : 0.0F;
    openness += is_open(tile_x + 1, tile_y - 1) ? diagonal_weight : 0.0F;
    openness += is_open(tile_x - 1, tile_y + 1) ? diagonal_weight : 0.0F;
    openness += is_open(tile_x + 1, tile_y + 1) ? diagonal_weight : 0.0F;

    const float total_weight = 4.0F + (4.0F * diagonal_weight);
    const float normalized_openness = total_weight <= 0.0F ? 0.0F : openness / total_weight;
    const float shaped_openness = std::pow(
        std::clamp(normalized_openness, 0.0F, 1.0F),
        std::max(state.settings.post_process.openness_ambient_gamma, 0.01F)
    );
    return Color3::White(std::clamp(base_ambient + (shaped_openness * openness_strength), 0.0F, 2.0F));
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

void ApplyLiveLightSourceAtWorldPos(
    const State& state,
    std::vector<std::vector<Color3>>& light_grid,
    const FVec2& world_pos,
    float source,
    Color3 color
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

            const FVec2 tile_center = FVec2::New(
                (static_cast<float>(base_x + offset_x) + 0.5F) * static_cast<float>(kTileSize),
                (static_cast<float>(base_y + offset_y) + 0.5F) * static_cast<float>(kTileSize)
            );
            const float distance_tiles =
                Length(tile_center - world_pos) / static_cast<float>(kTileSize);
            const float tile_center_source = std::max(
                0.0F,
                source - (distance_tiles * kLiveAirDecay)
            );
            Color3& light_value =
                light_grid[static_cast<std::size_t>(tile_pos.y)]
                          [static_cast<std::size_t>(tile_pos.x)];
            light_value = MaxColor(light_value, color * tile_center_source);
        }
    }
}

std::vector<LiveLightSource> BuildLiveLightSources(State& state) {
    std::vector<LiveLightSource> sources;
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return sources;
    }

    for (const StageLight& light : state.stage.lights) {
        if (light.radius <= 0) {
            continue;
        }
        const IVec2 center = state.stage.WrapTileCoord(light.tile_pos);
        if (!state.stage.IsTileCoordInside(center.x, center.y)) {
            continue;
        }

        sources.push_back(LiveLightSource{
            .world_pos = FVec2::New(
                (static_cast<float>(center.x) + 0.5F) * static_cast<float>(kTileSize),
                (static_cast<float>(center.y) + 0.5F) * static_cast<float>(kTileSize)
            ),
            .radius = light.radius,
            .source = kLiveStageLightSource + (static_cast<float>(light.radius) * 0.03F),
            .color = Color3::White(),
        });
    }

    for (const Ent& light_ent : state.ents.ents) {
        const float light_strength = ToFloat(light_ent.light_strength);
        if (!light_ent.active || !light_ent.render_enabled ||
            light_ent.condition == EntCondition::Dead ||
            light_strength <= 0.0F || light_ent.light_radius <= 0) {
            continue;
        }
        sources.push_back(LiveLightSource{
            .world_pos = ToFVec2(light_ent.GetCenter()),
            .radius = light_ent.light_radius,
            .source = light_strength,
            .color = ToFColor3(light_ent.light_color),
        });
    }

    for (const TransientLight& light : state.stage_lighting.transient_lights) {
        if (light.frames_remaining == 0 || light.total_frames == 0 ||
            light.strength <= 0.0F || light.radius <= 0) {
            continue;
        }
        const float fade =
            static_cast<float>(light.frames_remaining) / static_cast<float>(light.total_frames);
        sources.push_back(LiveLightSource{
            .world_pos = light.world_pos,
            .radius = light.radius,
            .source = light.strength * std::clamp(fade, 0.0F, 1.0F),
            .color = light.color,
        });
    }

    return sources;
}

void ApplyLiveLightSources(
    const State& state,
    const std::vector<LiveLightSource>& sources,
    std::vector<std::vector<Color3>>& light_grid
) {
    for (const LiveLightSource& source : sources) {
        if (source.radius <= 0 || source.source <= 0.0F) {
            continue;
        }
        ApplyLiveLightSourceAtWorldPos(state, light_grid, source.world_pos, source.source, source.color);
    }
}

void PropagateLiveStageLighting(
    State& state,
    const std::vector<LiveLightSource>& sources,
    std::vector<std::vector<Color3>>& light_grid
) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return;
    }

    int max_light_radius = 0;
    for (const LiveLightSource& source : sources) {
        max_light_radius = std::max(max_light_radius, source.radius);
    }
    const int pass_cap = std::max(kLiveMinimumPropagationPasses, stage_width + stage_height);
    const int passes = std::clamp(
        std::max(kLiveMinimumPropagationPasses, max_light_radius + 4),
        kLiveMinimumPropagationPasses,
        pass_cap
    );

    std::vector<std::vector<Color3>> next_grid = light_grid;
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
                const Color3 source = light_grid[static_cast<std::size_t>(y)]
                                               [static_cast<std::size_t>(x)];
                if (GetBrightnessFromColor(source) <= 0.001F) {
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

                    const float decay = GetLiveLightDecayIntoTile(state, neighbor.x, neighbor.y);
                    const Color3 candidate = Color3::New(
                        std::max(0.0F, source.r - decay),
                        std::max(0.0F, source.g - decay),
                        std::max(0.0F, source.b - decay)
                    );
                    if (GetBrightnessFromColor(candidate) <= 0.0F) {
                        continue;
                    }
                    Color3& target =
                        next_grid[static_cast<std::size_t>(neighbor.y)]
                                 [static_cast<std::size_t>(neighbor.x)];
                    target = MaxColor(target, candidate);
                }
            }
        }
        light_grid.swap(next_grid);
    }
}

std::vector<std::vector<Color3>> BuildLiveStageLightGrid(State& state) {
    const std::size_t tile_height = state.stage.tiles.size();
    const std::size_t tile_width =
        tile_height == 0 ? 0 : state.stage.tiles.front().size();
    std::vector<std::vector<Color3>> light_grid(
        tile_height,
        std::vector<Color3>(tile_width, Color3::White(0.0F))
    );

    for (std::size_t y = 0; y < tile_height; ++y) {
        for (std::size_t x = 0; x < tile_width; ++x) {
            light_grid[y][x] = GetLiveLightSeedForTile(state, static_cast<int>(x), static_cast<int>(y));
        }
    }

    const std::vector<LiveLightSource> sources = BuildLiveLightSources(state);
    ApplyLiveLightSources(state, sources, light_grid);
    PropagateLiveStageLighting(state, sources, light_grid);
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

Color3 ApplyForegroundLightingSettings(const State& state, Color3 light_color) {
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.terrain_exposure_lighting) {
        return Color3::White();
    }
    return Color3::New(
        ApplyForegroundLightingSettings(state, light_color.r),
        ApplyForegroundLightingSettings(state, light_color.g),
        ApplyForegroundLightingSettings(state, light_color.b)
    );
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

Color3 ApplyBackwallLightingSettings(const State& state, Color3 light_color, bool covered_by_solid) {
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.backwall_lighting) {
        return Color3::White();
    }
    return Color3::New(
        ApplyBackwallLightingSettings(state, light_color.r, covered_by_solid),
        ApplyBackwallLightingSettings(state, light_color.g, covered_by_solid),
        ApplyBackwallLightingSettings(state, light_color.b, covered_by_solid)
    );
}

} // namespace

VID AddStageLight(State& state, const IVec2& tile_pos, int radius) {
    const VID vid = state.stage.AddLight(tile_pos, radius);
    InvalidateStageLighting(state);
    return vid;
}

VID AddStageLightWithVid(State& state, VID vid, const IVec2& tile_pos, int radius) {
    const VID added_vid = state.stage.AddLightWithVid(vid, tile_pos, radius);
    InvalidateStageLighting(state);
    return added_vid;
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

    const std::vector<std::vector<Color3>> live_light_grid = BuildLiveStageLightGrid(state);
    const bool can_temporally_smooth =
        state.settings.post_process.lighting_temporal_smoothing &&
        state.stage_lighting.foreground_brightness.valid &&
        state.stage_lighting.backwall_base_brightness.valid;
    const float temporal_response = std::clamp(
        state.settings.post_process.lighting_temporal_smoothing_response,
        0.0F,
        1.0F
    );

    for (std::size_t y = 0; y < state.stage_lighting.foreground_topology.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage_lighting.foreground_topology.tiles[y].size(); ++x) {
            state.stage_lighting.foreground_topology.tiles[y][x] = BuildForegroundTileTopology(
                state,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            const Color3 live_light = live_light_grid[y][x];
            const Color3 foreground_brightness = ApplyForegroundLightingSettings(state, live_light);
            const Color3 backwall_brightness = ApplyBackwallLightingSettings(
                state,
                live_light,
                IsForegroundSolidTile(GetTileForLighting(state, static_cast<int>(x), static_cast<int>(y)))
            );
            if (can_temporally_smooth) {
                state.stage_lighting.foreground_brightness.tiles[y][x] = LerpColor(
                    state.stage_lighting.foreground_brightness.tiles[y][x],
                    foreground_brightness,
                    temporal_response
                );
                state.stage_lighting.backwall_base_brightness.tiles[y][x] = LerpColor(
                    state.stage_lighting.backwall_base_brightness.tiles[y][x],
                    backwall_brightness,
                    temporal_response
                );
            } else {
                state.stage_lighting.foreground_brightness.tiles[y][x] = foreground_brightness;
                state.stage_lighting.backwall_base_brightness.tiles[y][x] = backwall_brightness;
            }
            state.stage_lighting.backwall_light_brightness.tiles[y][x] = Color3::White(0.0F);
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

void AddTransientLight(
    State& state,
    const FVec2& world_pos,
    float strength,
    int radius,
    std::uint32_t lifetime_frames
) {
    AddTransientLight(state, world_pos, strength, Color3::White(), radius, lifetime_frames);
}

void AddTransientLight(
    State& state,
    const FVec2& world_pos,
    float strength,
    Color3 color,
    int radius,
    std::uint32_t lifetime_frames
) {
    if (strength <= 0.0F || radius <= 0 || lifetime_frames == 0) {
        return;
    }
    state.stage_lighting.transient_lights.push_back(TransientLight{
        .world_pos = world_pos,
        .strength = strength,
        .color = color,
        .radius = radius,
        .frames_remaining = lifetime_frames,
        .total_frames = lifetime_frames,
    });
    InvalidateStageLighting(state);
}

void StepTransientLights(State& state) {
    bool changed = false;
    for (TransientLight& light : state.stage_lighting.transient_lights) {
        if (light.frames_remaining > 0) {
            light.frames_remaining -= 1;
            changed = true;
        }
    }
    const auto removed_begin = std::remove_if(
        state.stage_lighting.transient_lights.begin(),
        state.stage_lighting.transient_lights.end(),
        [](const TransientLight& light) { return light.frames_remaining == 0; }
    );
    if (removed_begin != state.stage_lighting.transient_lights.end()) {
        state.stage_lighting.transient_lights.erase(
            removed_begin,
            state.stage_lighting.transient_lights.end()
        );
        changed = true;
    }
    if (changed) {
        InvalidateStageLighting(state);
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

Color3 GetForegroundLightColorForRender(const State& state, int tile_x, int tile_y) {
    const std::optional<IVec2> cached_tile =
        GetCachedLightingTilePosForRender(state, tile_x, tile_y);
    if (cached_tile.has_value() && state.stage_lighting.foreground_brightness.valid) {
        return state.stage_lighting.foreground_brightness
            .tiles[static_cast<std::size_t>(cached_tile->y)]
                  [static_cast<std::size_t>(cached_tile->x)];
    }

    return ApplyForegroundLightingSettings(state, GetLiveLightSeedForTile(state, tile_x, tile_y));
}

float GetForegroundBrightnessForRender(const State& state, int tile_x, int tile_y) {
    return GetBrightnessFromColor(GetForegroundLightColorForRender(state, tile_x, tile_y));
}

Color3 SampleLightColorForRender(
    const State& state,
    const FVec2& world_pos,
    Color3 (*sample_tile)(const State&, int, int)
) {
    const int stage_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_width <= 0 || stage_height <= 0) {
        return Color3::White();
    }

    const float tile_x = (world_pos.x / static_cast<float>(kTileSize)) - 0.5F;
    const float tile_y = (world_pos.y / static_cast<float>(kTileSize)) - 0.5F;
    const int base_x = static_cast<int>(std::floor(tile_x));
    const int base_y = static_cast<int>(std::floor(tile_y));
    const float frac_x = tile_x - static_cast<float>(base_x);
    const float frac_y = tile_y - static_cast<float>(base_y);

    Color3 weighted_color = Color3::White(0.0F);
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

            weighted_color = weighted_color + (sample_tile(state, tile_pos.x, tile_pos.y) * weight);
            total_weight += weight;
        }
    }

    if (total_weight <= 0.0F) {
        const IVec2 fallback_tile = state.stage.GetTileCoordAtWc(ToIVec2(world_pos));
        return sample_tile(state, fallback_tile.x, fallback_tile.y);
    }
    return weighted_color / total_weight;
}

float SampleBrightnessForRender(
    const State& state,
    const FVec2& world_pos,
    Color3 (*sample_tile)(const State&, int, int)
) {
    return GetBrightnessFromColor(SampleLightColorForRender(state, world_pos, sample_tile));
}

float SampleForegroundBrightnessForRender(const State& state, const FVec2& world_pos) {
    return SampleBrightnessForRender(state, world_pos, GetForegroundLightColorForRender);
}

Color3 SampleForegroundLightColorForRender(const State& state, const FVec2& world_pos) {
    return SampleLightColorForRender(state, world_pos, GetForegroundLightColorForRender);
}

Color3 GetBackwallLightColorForRender(const State& state, int tile_x, int tile_y) {
    const std::optional<IVec2> cached_tile =
        GetCachedLightingTilePosForRender(state, tile_x, tile_y);
    if (cached_tile.has_value() && state.stage_lighting.backwall_base_brightness.valid &&
        state.stage_lighting.backwall_light_brightness.valid) {
        return ClampColor(
            state.stage_lighting.backwall_base_brightness
                    .tiles[static_cast<std::size_t>(cached_tile->y)]
                          [static_cast<std::size_t>(cached_tile->x)] +
                state.stage_lighting.backwall_light_brightness
                    .tiles[static_cast<std::size_t>(cached_tile->y)]
                          [static_cast<std::size_t>(cached_tile->x)],
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

float GetBackwallBrightnessForRender(const State& state, int tile_x, int tile_y) {
    return GetBrightnessFromColor(GetBackwallLightColorForRender(state, tile_x, tile_y));
}

float SampleBackwallBrightnessForRender(const State& state, const FVec2& world_pos) {
    return SampleBrightnessForRender(state, world_pos, GetBackwallLightColorForRender);
}

Color3 SampleBackwallLightColorForRender(const State& state, const FVec2& world_pos) {
    return SampleLightColorForRender(state, world_pos, GetBackwallLightColorForRender);
}

} // namespace splonks

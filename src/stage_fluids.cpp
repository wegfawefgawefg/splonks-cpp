#include "stage_fluids.hpp"

#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace splonks {

namespace {

constexpr std::uint8_t kMaxFluidAmount = 255;

int WrapFluidCoordinate(int value, int size) {
    if (size <= 0) {
        return value;
    }
    int wrapped = value % size;
    if (wrapped < 0) {
        wrapped += size;
    }
    return wrapped;
}

bool IsSimulatedFluidTile(Tile tile) {
    return GetTileArchetype(tile).simulated_fluid;
}

bool IsFluidDestinationTile(Tile tile) {
    return tile == Tile::Air;
}

bool CanTerrainHoldFluid(Tile tile) {
    if (tile == Tile::Air) {
        return true;
    }
    const TileArchetype& archetype = GetTileArchetype(tile);
    return !archetype.simulated_fluid && archetype.transparent && !archetype.solid &&
           !archetype.one_way_top_solid;
}

std::optional<IVec2> ResolveFluidTileCoord(const Stage& stage, const IVec2& tile_coord) {
    IVec2 resolved = tile_coord;
    if (resolved.x < 0 || resolved.x >= static_cast<int>(stage.GetTileWidth())) {
        if (!stage.WrapsX()) {
            return std::nullopt;
        }
        resolved.x = WrapFluidCoordinate(resolved.x, static_cast<int>(stage.GetTileWidth()));
    }
    if (resolved.y < 0 || resolved.y >= static_cast<int>(stage.GetTileHeight())) {
        if (!stage.WrapsY()) {
            return std::nullopt;
        }
        resolved.y = WrapFluidCoordinate(resolved.y, static_cast<int>(stage.GetTileHeight()));
    }
    if (!stage.IsTileCoordInside(resolved.x, resolved.y)) {
        return std::nullopt;
    }
    return resolved;
}

std::int8_t GetMomentumFromGrid(
    const std::vector<std::vector<std::int8_t>>& momentum,
    const IVec2& tile_coord
) {
    if (tile_coord.y < 0 || static_cast<std::size_t>(tile_coord.y) >= momentum.size()) {
        return 0;
    }
    const std::vector<std::int8_t>& row = momentum[static_cast<std::size_t>(tile_coord.y)];
    if (tile_coord.x < 0 || static_cast<std::size_t>(tile_coord.x) >= row.size()) {
        return 0;
    }
    return std::clamp<std::int8_t>(row[static_cast<std::size_t>(tile_coord.x)], -1, 1);
}

void SetMomentumInGrid(
    std::vector<std::vector<std::int8_t>>& momentum,
    const IVec2& tile_coord,
    std::int8_t value
) {
    momentum[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)] =
        std::clamp<std::int8_t>(value, -1, 1);
}

Tile GetTileFromGrid(const std::vector<std::vector<Tile>>& tiles, const IVec2& tile_coord) {
    return tiles[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)];
}

void SetTileInGrid(std::vector<std::vector<Tile>>& tiles, const IVec2& tile_coord, Tile tile) {
    tiles[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)] = tile;
}

std::uint8_t GetAmountFromGrid(
    const std::vector<std::vector<std::uint8_t>>& amounts,
    const IVec2& tile_coord
) {
    return amounts[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)];
}

void SetAmountInGrid(
    std::vector<std::vector<std::uint8_t>>& amounts,
    const IVec2& tile_coord,
    std::uint8_t amount
) {
    amounts[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)] = amount;
}

void PushChangedTile(std::vector<IVec2>& changed_tiles, const IVec2& tile_coord) {
    if (std::find(changed_tiles.begin(), changed_tiles.end(), tile_coord) != changed_tiles.end()) {
        return;
    }
    changed_tiles.push_back(tile_coord);
}

std::optional<IVec2> ResolveFluidMoveTarget(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& terrain_tiles,
    const IVec2& start,
    const IVec2& direction
) {
    const std::optional<IVec2> resolved = ResolveFluidTileCoord(stage, start);
    if (!resolved.has_value()) {
        return std::nullopt;
    }
    if (!CanTerrainHoldFluid(GetTileFromGrid(terrain_tiles, *resolved))) {
        return std::nullopt;
    }
    (void)direction;
    return resolved;
}

bool TryMoveFluidTile(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& terrain_tiles,
    std::vector<std::vector<Tile>>& next_fluid_tiles,
    std::vector<std::vector<std::uint8_t>>& next_amount_grid,
    const IVec2& source,
    const IVec2& target,
    const IVec2& direction,
    Tile fluid_tile,
    std::uint8_t max_transfer,
    std::int8_t next_momentum,
    std::vector<IVec2>& changed_tiles,
    std::vector<std::vector<std::int8_t>>& next_momentum_grid
) {
    const std::optional<IVec2> resolved_target = ResolveFluidMoveTarget(
        stage,
        terrain_tiles,
        target,
        direction
    );
    if (!resolved_target.has_value()) {
        return false;
    }
    const Tile target_tile = GetTileFromGrid(next_fluid_tiles, *resolved_target);
    const std::uint8_t target_amount = GetAmountFromGrid(next_amount_grid, *resolved_target);
    if (target_amount > 0 && target_tile != fluid_tile) {
        return false;
    }

    const std::uint8_t source_amount = GetAmountFromGrid(next_amount_grid, source);
    const int capacity = static_cast<int>(kMaxFluidAmount) - static_cast<int>(target_amount);
    const int transfer = std::min({
        static_cast<int>(source_amount),
        capacity,
        static_cast<int>(max_transfer),
    });
    if (transfer <= 0) {
        return false;
    }

    const auto new_source_amount = static_cast<std::uint8_t>(
        static_cast<int>(source_amount) - transfer
    );
    const auto new_target_amount = static_cast<std::uint8_t>(
        static_cast<int>(target_amount) + transfer
    );

    SetAmountInGrid(next_amount_grid, source, new_source_amount);
    if (new_source_amount == 0) {
        SetTileInGrid(next_fluid_tiles, source, Tile::Air);
        SetMomentumInGrid(next_momentum_grid, source, 0);
    }
    SetTileInGrid(next_fluid_tiles, *resolved_target, fluid_tile);
    SetAmountInGrid(next_amount_grid, *resolved_target, new_target_amount);
    SetMomentumInGrid(next_momentum_grid, *resolved_target, next_momentum);
    PushChangedTile(changed_tiles, source);
    PushChangedTile(changed_tiles, *resolved_target);
    return true;
}

bool TryEqualizeFluidSideways(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& terrain_tiles,
    std::vector<std::vector<Tile>>& next_fluid_tiles,
    std::vector<std::vector<std::uint8_t>>& next_amount_grid,
    const IVec2& source,
    int direction,
    Tile fluid_tile,
    std::uint8_t horizontal_transfer_per_step,
    std::uint8_t horizontal_flow_deadband,
    std::vector<IVec2>& changed_tiles,
    std::vector<std::vector<std::int8_t>>& next_momentum_grid
) {
    const std::optional<IVec2> resolved_target = ResolveFluidMoveTarget(
        stage,
        terrain_tiles,
        source + IVec2::New(direction, 0),
        IVec2::New(direction, 0)
    );
    if (!resolved_target.has_value()) {
        return false;
    }

    const Tile target_tile = GetTileFromGrid(next_fluid_tiles, *resolved_target);
    const std::uint8_t target_amount = GetAmountFromGrid(next_amount_grid, *resolved_target);
    if (target_amount > 0 && target_tile != fluid_tile) {
        return false;
    }

    const std::uint8_t source_amount = GetAmountFromGrid(next_amount_grid, source);
    if (static_cast<int>(source_amount) <=
        static_cast<int>(target_amount) + static_cast<int>(horizontal_flow_deadband)) {
        return false;
    }

    const int capacity = static_cast<int>(kMaxFluidAmount) - static_cast<int>(target_amount);
    const int desired = (static_cast<int>(source_amount) - static_cast<int>(target_amount)) / 2;
    const int transfer = std::min({
        desired,
        capacity,
        static_cast<int>(horizontal_transfer_per_step),
    });
    if (transfer <= 0) {
        return false;
    }

    const auto new_source_amount = static_cast<std::uint8_t>(
        static_cast<int>(source_amount) - transfer
    );
    const auto new_target_amount = static_cast<std::uint8_t>(
        static_cast<int>(target_amount) + transfer
    );
    SetAmountInGrid(next_amount_grid, source, new_source_amount);
    if (new_source_amount == 0) {
        SetTileInGrid(next_fluid_tiles, source, Tile::Air);
        SetMomentumInGrid(next_momentum_grid, source, 0);
    }
    SetTileInGrid(next_fluid_tiles, *resolved_target, fluid_tile);
    SetAmountInGrid(next_amount_grid, *resolved_target, new_target_amount);
    SetMomentumInGrid(next_momentum_grid, *resolved_target, static_cast<std::int8_t>(direction));
    PushChangedTile(changed_tiles, source);
    PushChangedTile(changed_tiles, *resolved_target);
    return true;
}

bool TryMoveFluidTile(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& terrain_tiles,
    std::vector<std::vector<Tile>>& next_fluid_tiles,
    std::vector<std::vector<std::uint8_t>>& next_amount_grid,
    const std::vector<std::vector<std::int8_t>>& source_momentum_grid,
    std::vector<std::vector<std::int8_t>>& next_momentum_grid,
    const IVec2& source,
    Tile fluid_tile,
    std::uint8_t vertical_transfer_per_step,
    std::uint8_t horizontal_transfer_per_step,
    std::uint8_t horizontal_flow_deadband,
    bool left_first,
    std::vector<IVec2>& changed_tiles
) {
    const std::int8_t source_momentum = GetMomentumFromGrid(source_momentum_grid, source);
    if (TryMoveFluidTile(
            stage,
            terrain_tiles,
            next_fluid_tiles,
            next_amount_grid,
            source,
            source + IVec2::New(0, 1),
            IVec2::New(0, 1),
            fluid_tile,
            vertical_transfer_per_step,
            source_momentum,
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }

    const int first = source_momentum != 0 ? static_cast<int>(source_momentum)
                                           : (left_first ? -1 : 1);
    const int second = -first;
    if (TryEqualizeFluidSideways(
            stage,
            terrain_tiles,
            next_fluid_tiles,
            next_amount_grid,
            source,
            first,
            fluid_tile,
            horizontal_transfer_per_step,
            horizontal_flow_deadband,
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }
    if (TryEqualizeFluidSideways(
            stage,
            terrain_tiles,
            next_fluid_tiles,
            next_amount_grid,
            source,
            second,
            fluid_tile,
            horizontal_transfer_per_step,
            horizontal_flow_deadband,
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }

    if (source_momentum != 0) {
        SetMomentumInGrid(next_momentum_grid, source, 0);
    }
    return false;
}

std::vector<IVec2> NormalizeAuthoredFluidTiles(Stage& stage) {
    stage.SyncTileInstanceMetadataGrid();
    std::vector<IVec2> changed_tiles;
    for (int y = 0; y < static_cast<int>(stage.GetTileHeight()); ++y) {
        for (int x = 0; x < static_cast<int>(stage.GetTileWidth()); ++x) {
            const IVec2 tile_coord = IVec2::New(x, y);
            Tile& terrain_tile = stage.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (!IsSimulatedFluidTile(terrain_tile)) {
                continue;
            }

            Tile& fluid_tile = stage.fluid_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (fluid_tile == Tile::Air) {
                fluid_tile = terrain_tile;
            }
            stage.fluid_amount[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                kMaxFluidAmount;
            terrain_tile = Tile::Air;
            stage.tile_rotations[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                kTileRotation0;
            stage.fluid_momentum[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = 0;
            PushChangedTile(changed_tiles, tile_coord);
        }
    }
    if (!changed_tiles.empty()) {
        stage.tile_change_generation += 1;
    }
    return changed_tiles;
}

} // namespace

void StepStageFluids(State& state) {
    if (state.stage.tiles.empty()) {
        return;
    }
    std::vector<IVec2> normalized_tiles = NormalizeAuthoredFluidTiles(state.stage);
    if (!normalized_tiles.empty()) {
        UpdateStageLightingForTileChanges(state, normalized_tiles);
        UpdateStageAcousticsForTileChanges(state, normalized_tiles);
    }
    if (!state.debug_fluid_brush.simulation_enabled) {
        return;
    }
    const std::uint32_t interval_frames = static_cast<std::uint32_t>(
        std::max(1, state.debug_fluid_brush.simulation_interval_frames)
    );
    if (state.stage_frame % interval_frames != 0) {
        return;
    }

    state.stage.SyncTileInstanceMetadataGrid();
    const std::vector<std::vector<Tile>>& terrain_tiles = state.stage.tiles;
    const std::vector<std::vector<Tile>>& source_fluid_tiles = state.stage.fluid_tiles;
    const std::vector<std::vector<std::uint8_t>>& source_amount_grid = state.stage.fluid_amount;
    const std::vector<std::vector<std::int8_t>>& source_momentum_grid = state.stage.fluid_momentum;
    const auto vertical_transfer_per_step = static_cast<std::uint8_t>(std::clamp(
        state.debug_fluid_brush.vertical_transfer_per_step,
        0,
        static_cast<int>(kMaxFluidAmount)
    ));
    const auto horizontal_transfer_per_step = static_cast<std::uint8_t>(std::clamp(
        state.debug_fluid_brush.horizontal_transfer_per_step,
        0,
        static_cast<int>(kMaxFluidAmount)
    ));
    const auto horizontal_flow_deadband = static_cast<std::uint8_t>(std::clamp(
        state.debug_fluid_brush.horizontal_flow_deadband,
        0,
        static_cast<int>(kMaxFluidAmount)
    ));
    std::vector<std::vector<Tile>> next_fluid_tiles = source_fluid_tiles;
    std::vector<std::vector<std::uint8_t>> next_amount_grid = source_amount_grid;
    std::vector<std::vector<std::int8_t>> next_momentum_grid = source_momentum_grid;
    std::vector<IVec2> changed_tiles;
    const bool left_first = ((state.stage_frame / interval_frames) % 2U) == 0U;

    for (int y = static_cast<int>(state.stage.GetTileHeight()) - 1; y >= 0; --y) {
        for (int x = 0; x < static_cast<int>(state.stage.GetTileWidth()); ++x) {
            const IVec2 source = IVec2::New(x, y);
            const Tile fluid_tile = GetTileFromGrid(source_fluid_tiles, source);
            if (GetAmountFromGrid(source_amount_grid, source) == 0 ||
                !IsSimulatedFluidTile(fluid_tile) ||
                GetTileFromGrid(next_fluid_tiles, source) != fluid_tile) {
                continue;
            }
            (void)TryMoveFluidTile(
                state.stage,
                terrain_tiles,
                next_fluid_tiles,
                next_amount_grid,
                source_momentum_grid,
                next_momentum_grid,
                source,
                fluid_tile,
                vertical_transfer_per_step,
                horizontal_transfer_per_step,
                horizontal_flow_deadband,
                left_first,
                changed_tiles
            );
        }
    }

    if (changed_tiles.empty()) {
        state.stage.fluid_amount = std::move(next_amount_grid);
        state.stage.fluid_momentum = std::move(next_momentum_grid);
        return;
    }

    state.stage.fluid_tiles = std::move(next_fluid_tiles);
    state.stage.fluid_amount = std::move(next_amount_grid);
    state.stage.fluid_momentum = std::move(next_momentum_grid);
    state.stage.tile_change_generation += 1;
    UpdateStageLightingForTileChanges(state, changed_tiles);
    UpdateStageAcousticsForTileChanges(state, changed_tiles);
}

} // namespace splonks

#include "stage_fluids.hpp"

#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace splonks {

namespace {

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

bool IsFluidPassThroughTile(Tile tile) {
    const TileArchetype& archetype = GetTileArchetype(tile);
    return !archetype.simulated_fluid && archetype.transparent &&
           !archetype.solid && !archetype.one_way_top_solid &&
           (archetype.climbable || archetype.hangable);
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

void PushChangedTile(std::vector<IVec2>& changed_tiles, const IVec2& tile_coord) {
    if (std::find(changed_tiles.begin(), changed_tiles.end(), tile_coord) != changed_tiles.end()) {
        return;
    }
    changed_tiles.push_back(tile_coord);
}

bool CanFluidOccupy(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& source_tiles,
    const std::vector<std::vector<Tile>>& next_tiles,
    const IVec2& tile_coord
) {
    const std::optional<IVec2> resolved = ResolveFluidTileCoord(stage, tile_coord);
    if (!resolved.has_value()) {
        return false;
    }
    return IsFluidDestinationTile(GetTileFromGrid(source_tiles, *resolved)) &&
           IsFluidDestinationTile(GetTileFromGrid(next_tiles, *resolved));
}

std::optional<IVec2> ResolveFluidMoveTarget(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& source_tiles,
    const std::vector<std::vector<Tile>>& next_tiles,
    const IVec2& start,
    const IVec2& direction
) {
    const int max_steps = std::max(
        static_cast<int>(stage.GetTileWidth()),
        static_cast<int>(stage.GetTileHeight())
    );
    IVec2 candidate = start;
    for (int step = 0; step < max_steps; ++step) {
        const std::optional<IVec2> resolved = ResolveFluidTileCoord(stage, candidate);
        if (!resolved.has_value()) {
            return std::nullopt;
        }

        const Tile source_tile = GetTileFromGrid(source_tiles, *resolved);
        const Tile next_tile = GetTileFromGrid(next_tiles, *resolved);
        if (IsFluidDestinationTile(source_tile) && IsFluidDestinationTile(next_tile)) {
            return resolved;
        }
        if (!IsFluidPassThroughTile(source_tile) || source_tile != next_tile) {
            return std::nullopt;
        }

        candidate = candidate + direction;
    }
    return std::nullopt;
}

bool CanFluidDropFrom(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& source_tiles,
    const std::vector<std::vector<Tile>>& next_tiles,
    const IVec2& tile_coord
) {
    return ResolveFluidMoveTarget(
        stage,
        source_tiles,
        next_tiles,
        tile_coord + IVec2::New(0, 1),
        IVec2::New(0, 1)
    ).has_value();
}

bool HasFluidNeighbor(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& source_tiles,
    const IVec2& tile_coord,
    const IVec2& direction
) {
    const std::optional<IVec2> neighbor = ResolveFluidTileCoord(stage, tile_coord + direction);
    return neighbor.has_value() && IsSimulatedFluidTile(GetTileFromGrid(source_tiles, *neighbor));
}

bool TryMoveFluidTile(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& source_tiles,
    std::vector<std::vector<Tile>>& next_tiles,
    const IVec2& source,
    const IVec2& target,
    const IVec2& direction,
    Tile fluid_tile,
    bool require_drop_from_target,
    std::int8_t next_momentum,
    std::vector<IVec2>& changed_tiles,
    std::vector<std::vector<std::int8_t>>& next_momentum_grid
) {
    const std::optional<IVec2> resolved_target = ResolveFluidMoveTarget(
        stage,
        source_tiles,
        next_tiles,
        target,
        direction
    );
    if (!resolved_target.has_value()) {
        return false;
    }
    if (require_drop_from_target &&
        !CanFluidDropFrom(stage, source_tiles, next_tiles, *resolved_target)) {
        return false;
    }

    SetTileInGrid(next_tiles, source, Tile::Air);
    SetTileInGrid(next_tiles, *resolved_target, fluid_tile);
    SetMomentumInGrid(next_momentum_grid, source, 0);
    SetMomentumInGrid(next_momentum_grid, *resolved_target, next_momentum);
    PushChangedTile(changed_tiles, source);
    PushChangedTile(changed_tiles, *resolved_target);
    return true;
}

bool TryMoveFluidSideways(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& source_tiles,
    std::vector<std::vector<Tile>>& next_tiles,
    std::vector<std::vector<std::int8_t>>& next_momentum_grid,
    const IVec2& source,
    Tile fluid_tile,
    int direction,
    std::vector<IVec2>& changed_tiles
) {
    return TryMoveFluidTile(
        stage,
        source_tiles,
        next_tiles,
        source,
        source + IVec2::New(direction, 0),
        IVec2::New(direction, 0),
        fluid_tile,
        false,
        static_cast<std::int8_t>(direction),
        changed_tiles,
        next_momentum_grid
    );
}

bool TryMoveFluidTile(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& source_tiles,
    std::vector<std::vector<Tile>>& next_tiles,
    const std::vector<std::vector<std::int8_t>>& source_momentum_grid,
    std::vector<std::vector<std::int8_t>>& next_momentum_grid,
    const IVec2& source,
    Tile fluid_tile,
    bool left_first,
    std::vector<IVec2>& changed_tiles
) {
    const std::int8_t source_momentum = GetMomentumFromGrid(source_momentum_grid, source);
    if (TryMoveFluidTile(
            stage,
            source_tiles,
            next_tiles,
            source,
            source + IVec2::New(0, 1),
            IVec2::New(0, 1),
            fluid_tile,
            false,
            source_momentum,
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }

    const int first = source_momentum != 0 ? static_cast<int>(source_momentum)
                                           : (left_first ? -1 : 1);
    const int second = -first;
    if (TryMoveFluidTile(
            stage,
            source_tiles,
            next_tiles,
            source,
            source + IVec2::New(first, 1),
            IVec2::New(first, 1),
            fluid_tile,
            false,
            static_cast<std::int8_t>(first),
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }
    if (TryMoveFluidTile(
            stage,
            source_tiles,
            next_tiles,
            source,
            source + IVec2::New(second, 1),
            IVec2::New(second, 1),
            fluid_tile,
            false,
            static_cast<std::int8_t>(second),
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }

    if (source_momentum != 0 && TryMoveFluidTile(
            stage,
            source_tiles,
            next_tiles,
            source,
            source + IVec2::New(static_cast<int>(source_momentum), 0),
            IVec2::New(static_cast<int>(source_momentum), 0),
            fluid_tile,
            false,
            source_momentum,
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }

    const bool has_left_fluid =
        HasFluidNeighbor(stage, source_tiles, source, IVec2::New(-1, 0));
    const bool has_right_fluid =
        HasFluidNeighbor(stage, source_tiles, source, IVec2::New(1, 0));
    const bool can_equalize = has_left_fluid != has_right_fluid;
    if (can_equalize && TryMoveFluidSideways(
            stage,
            source_tiles,
            next_tiles,
            next_momentum_grid,
            source,
            fluid_tile,
            has_left_fluid ? 1 : -1,
            changed_tiles
        )) {
        return true;
    }

    if (TryMoveFluidTile(
            stage,
            source_tiles,
            next_tiles,
            source,
            source + IVec2::New(first, 0),
            IVec2::New(first, 0),
            fluid_tile,
            true,
            static_cast<std::int8_t>(first),
            changed_tiles,
            next_momentum_grid
        )) {
        return true;
    }
    if (TryMoveFluidTile(
        stage,
        source_tiles,
        next_tiles,
        source,
        source + IVec2::New(second, 0),
        IVec2::New(second, 0),
        fluid_tile,
        true,
        static_cast<std::int8_t>(second),
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

} // namespace

void StepStageFluids(State& state) {
    if (!state.debug_fluid_brush.simulation_enabled ||
        state.stage.tiles.empty()) {
        return;
    }
    const std::uint32_t interval_frames = static_cast<std::uint32_t>(
        std::max(1, state.debug_fluid_brush.simulation_interval_frames)
    );
    if (state.stage_frame % interval_frames != 0) {
        return;
    }

    state.stage.SyncFluidMomentumGrid();
    const std::vector<std::vector<Tile>>& source_tiles = state.stage.tiles;
    const std::vector<std::vector<std::int8_t>>& source_momentum_grid = state.stage.fluid_momentum;
    std::vector<std::vector<Tile>> next_tiles = source_tiles;
    std::vector<std::vector<std::int8_t>> next_momentum_grid = source_momentum_grid;
    std::vector<IVec2> changed_tiles;
    const bool left_first = ((state.stage_frame / interval_frames) % 2U) == 0U;

    for (int y = static_cast<int>(state.stage.GetTileHeight()) - 1; y >= 0; --y) {
        for (int x = 0; x < static_cast<int>(state.stage.GetTileWidth()); ++x) {
            const IVec2 source = IVec2::New(x, y);
            const Tile fluid_tile = GetTileFromGrid(source_tiles, source);
            if (!IsSimulatedFluidTile(fluid_tile) ||
                GetTileFromGrid(next_tiles, source) != fluid_tile) {
                continue;
            }
            (void)TryMoveFluidTile(
                state.stage,
                source_tiles,
                next_tiles,
                source_momentum_grid,
                next_momentum_grid,
                source,
                fluid_tile,
                left_first,
                changed_tiles
            );
        }
    }

    if (changed_tiles.empty()) {
        state.stage.fluid_momentum = std::move(next_momentum_grid);
        return;
    }

    for (const IVec2& tile_coord : changed_tiles) {
        state.stage.SetTile(tile_coord, GetTileFromGrid(next_tiles, tile_coord));
    }
    state.stage.fluid_momentum = std::move(next_momentum_grid);
    UpdateStageLightingForTileChanges(state, changed_tiles);
    UpdateStageAcousticsForTileChanges(state, changed_tiles);
}

} // namespace splonks

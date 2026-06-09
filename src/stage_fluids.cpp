#include "stage_fluids.hpp"

#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "tile_spec.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace splonks {

namespace {

constexpr FxScalar kMaxFluidAmount = FxScalar::from_int(1);
constexpr FxScalar kZero = FxScalar::zero();
constexpr FxScalar kOne = FxScalar::from_int(1);
constexpr FxScalar kFour = FxScalar::from_int(4);
const FxScalar kMinFluidAmount =
    ToFxScalar(0.0001F, gfxp::Rounding::Ceil);
const FxScalar kVelocityClamp = FxScalar::from_int(8);
const FxScalar kGravityEpsilon =
    ToFxScalar(0.0001F, gfxp::Rounding::Ceil);

struct FluidTransferProposal {
    IVec2 source = IVec2::New(0, 0);
    IVec2 target = IVec2::New(0, 0);
    Tile fluid_tile = Tile::Air;
    FxScalar amount = FxScalar::zero();
    FxVec2 direction = FxVec2::zero();
};

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

FxScalar Dot(FxVec2 left, FxVec2 right) {
    return (left.x * right.x) + (left.y * right.y);
}

FxScalar ClampScalar(FxScalar value, FxScalar min_value, FxScalar max_value) {
    return std::clamp(value, min_value, max_value);
}

FxVec2 ClampLength(FxVec2 value, FxScalar max_length) {
    const FxScalar length = FxLength(value);
    if (length <= max_length || length <= kZero) {
        return value;
    }
    return value * (max_length / length);
}

bool IsSimulatedFluidTile(Tile tile) {
    return GetTileSpec(tile).simulated_fluid;
}

bool CanTerrainHoldFluid(Tile tile) {
    if (tile == Tile::Air) {
        return true;
    }
    const TileSpec& spec = GetTileSpec(tile);
    return !spec.simulated_fluid && spec.transparent && !spec.solid &&
           !spec.one_way_top_solid;
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

Tile GetTileFromGrid(const std::vector<std::vector<Tile>>& tiles, const IVec2& tile_coord) {
    return tiles[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)];
}

FxScalar GetAmountFromGrid(
    const std::vector<std::vector<FxScalar>>& amounts,
    const IVec2& tile_coord
) {
    return amounts[static_cast<std::size_t>(tile_coord.y)]
                  [static_cast<std::size_t>(tile_coord.x)];
}

void SetAmountInGrid(
    std::vector<std::vector<FxScalar>>& amounts,
    const IVec2& tile_coord,
    FxScalar amount
) {
    amounts[static_cast<std::size_t>(tile_coord.y)]
           [static_cast<std::size_t>(tile_coord.x)] = amount;
}

FxVec2 GetVelocityFromGrid(
    const std::vector<std::vector<FxVec2>>& velocities,
    const IVec2& tile_coord
) {
    return velocities[static_cast<std::size_t>(tile_coord.y)]
                     [static_cast<std::size_t>(tile_coord.x)];
}

FxVec2 GetVec2FromGrid(
    const std::vector<std::vector<FxVec2>>& grid,
    const IVec2& tile_coord
) {
    return grid[static_cast<std::size_t>(tile_coord.y)]
               [static_cast<std::size_t>(tile_coord.x)];
}

void SetVec2InGrid(
    std::vector<std::vector<FxVec2>>& grid,
    const IVec2& tile_coord,
    FxVec2 value
) {
    grid[static_cast<std::size_t>(tile_coord.y)]
        [static_cast<std::size_t>(tile_coord.x)] = value;
}

void PushChangedTile(std::vector<IVec2>& changed_tiles, const IVec2& tile_coord) {
    if (std::find(changed_tiles.begin(), changed_tiles.end(), tile_coord) != changed_tiles.end()) {
        return;
    }
    changed_tiles.push_back(tile_coord);
}

FxScalar GetTargetCapacity(
    const std::vector<std::vector<Tile>>& fluid_tiles,
    const std::vector<std::vector<FxScalar>>& amounts,
    const IVec2& target,
    Tile fluid_tile
) {
    const FxScalar target_amount = GetAmountFromGrid(amounts, target);
    const Tile target_tile = GetTileFromGrid(fluid_tiles, target);
    if (target_amount > kMinFluidAmount && target_tile != fluid_tile) {
        return kZero;
    }
    const FxScalar capacity = kMaxFluidAmount - target_amount;
    return capacity > kZero ? capacity : kZero;
}

void AddFluidTransferProposalsForCell(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& terrain_tiles,
    const std::vector<std::vector<Tile>>& fluid_tiles,
    const std::vector<std::vector<FxScalar>>& amounts,
    const std::vector<std::vector<FxVec2>>& velocities,
    const std::vector<std::vector<FxVec2>>& gravity_overrides,
    const std::vector<std::vector<std::uint8_t>>& gravity_strengths,
    const std::vector<std::vector<FxVec2>>& temp_gravity,
    const IVec2& source,
    FxScalar transfer_cap,
    FxScalar pressure_strength,
    FxVec2 gravity,
    std::vector<FluidTransferProposal>& proposals
) {
    const Tile fluid_tile = GetTileFromGrid(fluid_tiles, source);
    const FxScalar source_amount = GetAmountFromGrid(amounts, source);
    if (source_amount <= kMinFluidAmount || !IsSimulatedFluidTile(fluid_tile)) {
        return;
    }

    struct Candidate {
        IVec2 target = IVec2::New(0, 0);
        FxVec2 direction = FxVec2::zero();
        FxScalar score = FxScalar::zero();
        FxScalar capacity = FxScalar::zero();
    };

    const IVec2 neighbor_offsets[] = {
        IVec2::New(-1, -1),
        IVec2::New(0, -1),
        IVec2::New(1, -1),
        IVec2::New(-1, 0),
        IVec2::New(1, 0),
        IVec2::New(-1, 1),
        IVec2::New(0, 1),
        IVec2::New(1, 1),
    };

    const bool gravity_override_active =
        gravity_strengths[static_cast<std::size_t>(source.y)][static_cast<std::size_t>(source.x)];
    const FxVec2 effective_gravity = (gravity_override_active
        ? GetVec2FromGrid(gravity_overrides, source)
        : gravity) + GetVec2FromGrid(temp_gravity, source);
    const FxVec2 source_velocity = ClampLength(
        GetVelocityFromGrid(velocities, source) + effective_gravity,
        kVelocityClamp
    );
    const FxScalar gravity_magnitude = FxLength(effective_gravity);
    const FxVec2 gravity_direction = FxNormalizeOrZero(effective_gravity);
    const bool has_gravity = gravity_magnitude > kGravityEpsilon;
    const FxScalar gravity_pressure_bias = ClampScalar(gravity_magnitude, kZero, kOne);
    std::vector<Candidate> candidates;
    FxScalar total_score = FxScalar::zero();

    for (const IVec2& offset : neighbor_offsets) {
        const std::optional<IVec2> resolved_target =
            ResolveFluidTileCoord(stage, source + offset);
        if (!resolved_target.has_value()) {
            continue;
        }
        if (!CanTerrainHoldFluid(GetTileFromGrid(terrain_tiles, *resolved_target))) {
            continue;
        }

        const FxScalar target_capacity =
            GetTargetCapacity(fluid_tiles, amounts, *resolved_target, fluid_tile);
        if (target_capacity <= kMinFluidAmount) {
            continue;
        }

        const FxScalar target_amount = GetAmountFromGrid(amounts, *resolved_target);
        const FxVec2 direction = FxNormalizeOrZero(FxVec2::from_int(offset.x, offset.y));
        const FxScalar velocity_score =
            ClampScalar(Dot(source_velocity, direction), kZero, kVelocityClamp);
        const FxScalar directional_pressure_gate =
            (!has_gravity || Dot(direction, gravity_direction) >= ToFxScalar(-0.05F))
            ? kOne
            : kZero;
        const FxScalar pressure_gate =
            ((kOne - gravity_pressure_bias) + (gravity_pressure_bias * directional_pressure_gate));
        const FxScalar amount_delta = source_amount - target_amount;
        const FxScalar pressure_score =
            (amount_delta > kZero ? amount_delta : kZero) *
            pressure_strength *
            pressure_gate;
        const FxScalar score = velocity_score + pressure_score;
        if (score <= kZero) {
            continue;
        }

        candidates.push_back(Candidate{
            .target = *resolved_target,
            .direction = direction,
            .score = score,
            .capacity = target_capacity,
        });
        total_score += score;
    }

    if (total_score <= kZero || candidates.empty()) {
        return;
    }

    const FxScalar source_budget = std::min(
        source_amount,
        transfer_cap * ClampScalar(total_score, kZero, kOne)
    );
    for (const Candidate& candidate : candidates) {
        const FxScalar requested = std::min(
            candidate.capacity,
            source_budget * (candidate.score / total_score)
        );
        if (requested <= kZero) {
            continue;
        }
        proposals.push_back(FluidTransferProposal{
            .source = source,
            .target = candidate.target,
            .fluid_tile = fluid_tile,
            .amount = requested,
            .direction = candidate.direction,
        });
    }
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

            Tile& fluid_tile = stage.fluid_tiles[static_cast<std::size_t>(y)]
                                                [static_cast<std::size_t>(x)];
            if (fluid_tile == Tile::Air) {
                fluid_tile = terrain_tile;
            }
            stage.fluid_amount[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                FxScalar::from_int(1);
            stage.fluid_display_amount[static_cast<std::size_t>(y)]
                                      [static_cast<std::size_t>(x)] = FxScalar::from_int(1);
            stage.fluid_velocity[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                FxVec2::zero();
            terrain_tile = Tile::Air;
            stage.tile_rotations[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                kTileRotation0;
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
    const FluidSettings& fluid = state.settings.fluid;
    if (!fluid.simulation_enabled) {
        return;
    }

    const std::uint32_t interval_frames = static_cast<std::uint32_t>(
        std::max(1, fluid.simulation_interval_frames)
    );
    if (state.stage_frame % interval_frames != 0) {
        return;
    }

    Stage& stage = state.stage;
    stage.SyncTileInstanceMetadataGrid();
    const std::vector<std::vector<Tile>>& terrain_tiles = stage.tiles;
    const std::vector<std::vector<Tile>>& source_fluid_tiles = stage.fluid_tiles;
    const std::vector<std::vector<FxScalar>>& source_amounts = stage.fluid_amount;
    const std::vector<std::vector<FxVec2>>& source_velocities = stage.fluid_velocity;
    const std::vector<std::vector<FxVec2>>& source_gravity_overrides = stage.fluid_gravity;
    const std::vector<std::vector<std::uint8_t>>& source_gravity_strengths =
        stage.fluid_gravity_strength;
    const std::vector<std::vector<FxVec2>>& source_temp_gravity = stage.fluid_temp_gravity;

    const FxScalar transfer_cap =
        ClampScalar(fluid.transfer_per_step, kZero, kMaxFluidAmount);
    const FxScalar pressure_strength = ClampScalar(fluid.pressure_strength, kZero, kFour);
    const FxScalar velocity_damping_sim = ClampScalar(fluid.velocity_damping, kZero, kOne);
    const FxScalar temp_gravity_decay_sim =
        ClampScalar(fluid.temp_gravity_decay, kZero, kOne);
    const FxVec2 gravity{fluid.gravity_x, fluid.gravity_y};

    std::vector<FluidTransferProposal> proposals;
    proposals.reserve(static_cast<std::size_t>(stage.GetTileWidth() * stage.GetTileHeight()));

    for (int y = 0; y < static_cast<int>(stage.GetTileHeight()); ++y) {
        for (int x = 0; x < static_cast<int>(stage.GetTileWidth()); ++x) {
            AddFluidTransferProposalsForCell(
                stage,
                terrain_tiles,
                source_fluid_tiles,
                source_amounts,
                source_velocities,
                source_gravity_overrides,
                source_gravity_strengths,
                source_temp_gravity,
                IVec2::New(x, y),
                transfer_cap,
                pressure_strength,
                gravity,
                proposals
            );
        }
    }

    std::vector<std::vector<FxScalar>> incoming_capacity_used(
        source_amounts.size(),
        std::vector<FxScalar>(
            source_amounts.empty() ? 0 : source_amounts.front().size(),
            FxScalar::zero()
        )
    );
    for (const FluidTransferProposal& proposal : proposals) {
        incoming_capacity_used[static_cast<std::size_t>(proposal.target.y)]
                              [static_cast<std::size_t>(proposal.target.x)] += proposal.amount;
    }

    std::vector<std::vector<Tile>> next_fluid_tiles = source_fluid_tiles;
    std::vector<std::vector<FxScalar>> next_amounts = source_amounts;
    std::vector<std::vector<FxVec2>> next_velocities = source_velocities;
    std::vector<std::vector<FxVec2>> next_temp_gravity = source_temp_gravity;
    std::vector<std::vector<FxVec2>> incoming_velocity(
        source_amounts.size(),
        std::vector<FxVec2>(
            source_amounts.empty() ? 0 : source_amounts.front().size(),
            FxVec2::zero()
        )
    );

    for (std::size_t y = 0; y < next_velocities.size(); ++y) {
        for (std::size_t x = 0; x < next_velocities[y].size(); ++x) {
            next_temp_gravity[y][x] *= temp_gravity_decay_sim;
            if (source_amounts[y][x] <= kMinFluidAmount) {
                next_velocities[y][x] = FxVec2::zero();
                continue;
            }
            const FxVec2 cell_gravity = ((source_gravity_strengths[y][x] > 0)
                ? source_gravity_overrides[y][x]
                : gravity) + source_temp_gravity[y][x];
            next_velocities[y][x] = ClampLength(
                (next_velocities[y][x] + cell_gravity) * velocity_damping_sim,
                kVelocityClamp
            );
        }
    }

    for (const FluidTransferProposal& proposal : proposals) {
        const FxScalar capacity = GetTargetCapacity(
            source_fluid_tiles,
            source_amounts,
            proposal.target,
            proposal.fluid_tile
        );
        const FxScalar incoming =
            incoming_capacity_used[static_cast<std::size_t>(proposal.target.y)]
                                  [static_cast<std::size_t>(proposal.target.x)];
        const FxScalar target_scale = incoming > capacity && incoming > kZero
            ? capacity / incoming
            : kOne;
        const FxScalar amount = std::min(
            proposal.amount * target_scale,
            GetAmountFromGrid(next_amounts, proposal.source)
        );
        if (amount <= kZero) {
            continue;
        }

        SetAmountInGrid(
            next_amounts,
            proposal.source,
            GetAmountFromGrid(next_amounts, proposal.source) - amount
        );
        SetAmountInGrid(
            next_amounts,
            proposal.target,
            GetAmountFromGrid(next_amounts, proposal.target) + amount
        );
        next_fluid_tiles[static_cast<std::size_t>(proposal.target.y)]
                        [static_cast<std::size_t>(proposal.target.x)] = proposal.fluid_tile;
        incoming_velocity[static_cast<std::size_t>(proposal.target.y)]
                         [static_cast<std::size_t>(proposal.target.x)] +=
            proposal.direction * amount;
    }

    std::vector<IVec2> changed_tiles;
    for (int y = 0; y < static_cast<int>(stage.GetTileHeight()); ++y) {
        for (int x = 0; x < static_cast<int>(stage.GetTileWidth()); ++x) {
            const IVec2 tile_coord = IVec2::New(x, y);
            FxScalar next_amount = GetAmountFromGrid(next_amounts, tile_coord);
            Tile& next_tile = next_fluid_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            FxVec2 next_velocity =
                next_velocities[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            next_amount = ClampScalar(next_amount, kZero, kMaxFluidAmount);
            next_velocity = ClampLength(
                (next_velocity + incoming_velocity[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) *
                    velocity_damping_sim,
                kVelocityClamp
            );

            if (next_amount <= kMinFluidAmount || !CanTerrainHoldFluid(GetTileFromGrid(terrain_tiles, tile_coord))) {
                next_amount = kZero;
                next_tile = Tile::Air;
                next_velocity = FxVec2::zero();
            }
            SetAmountInGrid(next_amounts, tile_coord, next_amount);
            SetVec2InGrid(next_velocities, tile_coord, next_velocity);

            const bool changed =
                next_tile !=
                    source_fluid_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] ||
                (next_amount -
                 source_amounts[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]).abs() >
                    kMinFluidAmount;
            if (changed) {
                PushChangedTile(changed_tiles, tile_coord);
            }
        }
    }

    stage.fluid_tiles = std::move(next_fluid_tiles);
    stage.fluid_amount = std::move(next_amounts);
    stage.fluid_velocity = std::move(next_velocities);
    stage.fluid_temp_gravity = std::move(next_temp_gravity);
    if (!changed_tiles.empty()) {
        stage.tile_change_generation += 1;
        UpdateStageLightingForTileChanges(state, changed_tiles);
        UpdateStageAcousticsForTileChanges(state, changed_tiles);
    }
}

} // namespace splonks

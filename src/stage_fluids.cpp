#include "stage_fluids.hpp"

#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace splonks {

namespace {

constexpr float kMaxFluidAmount = 255.0F;
constexpr float kMinFluidAmount = 0.25F;
constexpr float kVelocityClamp = 8.0F;

struct FluidTransferProposal {
    IVec2 source = IVec2::New(0, 0);
    IVec2 target = IVec2::New(0, 0);
    Tile fluid_tile = Tile::Air;
    float amount = 0.0F;
    Vec2 direction = Vec2::New(0.0F, 0.0F);
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

float Dot(const Vec2& left, const Vec2& right) {
    return (left.x * right.x) + (left.y * right.y);
}

Vec2 ClampLength(const Vec2& value, float max_length) {
    const float length = Length(value);
    if (length <= max_length || length <= 0.0F) {
        return value;
    }
    return value * (max_length / length);
}

bool IsSimulatedFluidTile(Tile tile) {
    return GetTileArchetype(tile).simulated_fluid;
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

Tile GetTileFromGrid(const std::vector<std::vector<Tile>>& tiles, const IVec2& tile_coord) {
    return tiles[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)];
}

float GetAmountFromGrid(const std::vector<std::vector<float>>& amounts, const IVec2& tile_coord) {
    return amounts[static_cast<std::size_t>(tile_coord.y)][static_cast<std::size_t>(tile_coord.x)];
}

Vec2 GetVelocityFromGrid(const std::vector<std::vector<Vec2>>& velocities, const IVec2& tile_coord) {
    return velocities[static_cast<std::size_t>(tile_coord.y)]
                     [static_cast<std::size_t>(tile_coord.x)];
}

void PushChangedTile(std::vector<IVec2>& changed_tiles, const IVec2& tile_coord) {
    if (std::find(changed_tiles.begin(), changed_tiles.end(), tile_coord) != changed_tiles.end()) {
        return;
    }
    changed_tiles.push_back(tile_coord);
}

float GetTargetCapacity(
    const std::vector<std::vector<Tile>>& fluid_tiles,
    const std::vector<std::vector<float>>& amounts,
    const IVec2& target,
    Tile fluid_tile
) {
    const float target_amount = GetAmountFromGrid(amounts, target);
    const Tile target_tile = GetTileFromGrid(fluid_tiles, target);
    if (target_amount > kMinFluidAmount && target_tile != fluid_tile) {
        return 0.0F;
    }
    return std::max(0.0F, kMaxFluidAmount - target_amount);
}

void AddFluidTransferProposalsForCell(
    const Stage& stage,
    const std::vector<std::vector<Tile>>& terrain_tiles,
    const std::vector<std::vector<Tile>>& fluid_tiles,
    const std::vector<std::vector<float>>& amounts,
    const std::vector<std::vector<Vec2>>& velocities,
    const IVec2& source,
    float transfer_cap,
    float pressure_strength,
    const Vec2& gravity,
    std::vector<FluidTransferProposal>& proposals
) {
    const Tile fluid_tile = GetTileFromGrid(fluid_tiles, source);
    const float source_amount = GetAmountFromGrid(amounts, source);
    if (source_amount <= kMinFluidAmount || !IsSimulatedFluidTile(fluid_tile)) {
        return;
    }

    struct Candidate {
        IVec2 target = IVec2::New(0, 0);
        Vec2 direction = Vec2::New(0.0F, 0.0F);
        float score = 0.0F;
        float capacity = 0.0F;
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

    const Vec2 source_velocity = ClampLength(
        GetVelocityFromGrid(velocities, source) + gravity,
        kVelocityClamp
    );
    const Vec2 gravity_direction = NormalizeOrZero(gravity);
    const bool has_gravity = Length(gravity_direction) > 0.0F;
    std::vector<Candidate> candidates;
    float total_score = 0.0F;

    for (const IVec2& offset : neighbor_offsets) {
        const std::optional<IVec2> resolved_target =
            ResolveFluidTileCoord(stage, source + offset);
        if (!resolved_target.has_value()) {
            continue;
        }
        if (!CanTerrainHoldFluid(GetTileFromGrid(terrain_tiles, *resolved_target))) {
            continue;
        }

        const float target_capacity =
            GetTargetCapacity(fluid_tiles, amounts, *resolved_target, fluid_tile);
        if (target_capacity <= kMinFluidAmount) {
            continue;
        }

        const float target_amount = GetAmountFromGrid(amounts, *resolved_target);
        const Vec2 direction = NormalizeOrZero(ToVec2(offset));
        const float velocity_score = std::max(0.0F, Dot(source_velocity, direction));
        const bool pressure_can_flow = !has_gravity || Dot(direction, gravity_direction) >= -0.05F;
        const float pressure_score = pressure_can_flow
            ? std::max(0.0F, source_amount - target_amount) * pressure_strength / kMaxFluidAmount
            : 0.0F;
        const float score = velocity_score + pressure_score;
        if (score <= 0.0F) {
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

    if (total_score <= 0.0F || candidates.empty()) {
        return;
    }

    const float source_budget = std::min(source_amount, transfer_cap);
    for (const Candidate& candidate : candidates) {
        const float requested = std::min(
            candidate.capacity,
            source_budget * (candidate.score / total_score)
        );
        if (requested <= kMinFluidAmount) {
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
                kMaxFluidAmount;
            stage.fluid_display_amount[static_cast<std::size_t>(y)]
                                      [static_cast<std::size_t>(x)] = 1.0F;
            stage.fluid_velocity[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                Vec2::New(0.0F, 0.0F);
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
    if (!state.debug_fluid_brush.simulation_enabled) {
        return;
    }

    const std::uint32_t interval_frames = static_cast<std::uint32_t>(
        std::max(1, state.debug_fluid_brush.simulation_interval_frames)
    );
    if (state.stage_frame % interval_frames != 0) {
        return;
    }

    Stage& stage = state.stage;
    stage.SyncTileInstanceMetadataGrid();
    const std::vector<std::vector<Tile>>& terrain_tiles = stage.tiles;
    const std::vector<std::vector<Tile>>& source_fluid_tiles = stage.fluid_tiles;
    const std::vector<std::vector<float>>& source_amounts = stage.fluid_amount;
    const std::vector<std::vector<Vec2>>& source_velocities = stage.fluid_velocity;

    const float transfer_cap = std::clamp(
        static_cast<float>(state.debug_fluid_brush.transfer_per_step),
        0.0F,
        kMaxFluidAmount
    );
    const float pressure_strength = std::clamp(
        state.debug_fluid_brush.pressure_strength,
        0.0F,
        4.0F
    );
    const float velocity_damping = std::clamp(
        state.debug_fluid_brush.velocity_damping,
        0.0F,
        1.0F
    );
    const Vec2 gravity = Vec2::New(
        state.debug_fluid_brush.gravity_x,
        state.debug_fluid_brush.gravity_y
    );

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
                IVec2::New(x, y),
                transfer_cap,
                pressure_strength,
                gravity,
                proposals
            );
        }
    }

    std::vector<std::vector<float>> incoming_capacity_used(
        source_amounts.size(),
        std::vector<float>(source_amounts.empty() ? 0 : source_amounts.front().size(), 0.0F)
    );
    for (const FluidTransferProposal& proposal : proposals) {
        incoming_capacity_used[static_cast<std::size_t>(proposal.target.y)]
                              [static_cast<std::size_t>(proposal.target.x)] += proposal.amount;
    }

    std::vector<std::vector<Tile>> next_fluid_tiles = source_fluid_tiles;
    std::vector<std::vector<float>> next_amounts = source_amounts;
    std::vector<std::vector<Vec2>> next_velocities = source_velocities;
    std::vector<std::vector<Vec2>> incoming_velocity(
        source_amounts.size(),
        std::vector<Vec2>(
            source_amounts.empty() ? 0 : source_amounts.front().size(),
            Vec2::New(0.0F, 0.0F)
        )
    );

    for (std::size_t y = 0; y < next_velocities.size(); ++y) {
        for (std::size_t x = 0; x < next_velocities[y].size(); ++x) {
            if (source_amounts[y][x] <= kMinFluidAmount) {
                next_velocities[y][x] = Vec2::New(0.0F, 0.0F);
                continue;
            }
            next_velocities[y][x] =
                ClampLength((next_velocities[y][x] + gravity) * velocity_damping, kVelocityClamp);
        }
    }

    for (const FluidTransferProposal& proposal : proposals) {
        const float capacity = GetTargetCapacity(
            source_fluid_tiles,
            source_amounts,
            proposal.target,
            proposal.fluid_tile
        );
        const float incoming = incoming_capacity_used[static_cast<std::size_t>(proposal.target.y)]
                                             [static_cast<std::size_t>(proposal.target.x)];
        const float target_scale = incoming > capacity && incoming > 0.0F
            ? capacity / incoming
            : 1.0F;
        const float amount = std::min(proposal.amount * target_scale, GetAmountFromGrid(next_amounts, proposal.source));
        if (amount <= kMinFluidAmount) {
            continue;
        }

        next_amounts[static_cast<std::size_t>(proposal.source.y)]
                    [static_cast<std::size_t>(proposal.source.x)] -= amount;
        next_amounts[static_cast<std::size_t>(proposal.target.y)]
                    [static_cast<std::size_t>(proposal.target.x)] += amount;
        next_fluid_tiles[static_cast<std::size_t>(proposal.target.y)]
                        [static_cast<std::size_t>(proposal.target.x)] = proposal.fluid_tile;
        incoming_velocity[static_cast<std::size_t>(proposal.target.y)]
                         [static_cast<std::size_t>(proposal.target.x)] +=
            proposal.direction * (amount / kMaxFluidAmount);
    }

    std::vector<IVec2> changed_tiles;
    for (int y = 0; y < static_cast<int>(stage.GetTileHeight()); ++y) {
        for (int x = 0; x < static_cast<int>(stage.GetTileWidth()); ++x) {
            const IVec2 tile_coord = IVec2::New(x, y);
            float& next_amount = next_amounts[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            Tile& next_tile = next_fluid_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            Vec2& next_velocity = next_velocities[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            next_amount = std::clamp(next_amount, 0.0F, kMaxFluidAmount);
            next_velocity = ClampLength(
                (next_velocity + incoming_velocity[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) *
                    velocity_damping,
                kVelocityClamp
            );

            if (next_amount <= kMinFluidAmount || !CanTerrainHoldFluid(GetTileFromGrid(terrain_tiles, tile_coord))) {
                next_amount = 0.0F;
                next_tile = Tile::Air;
                next_velocity = Vec2::New(0.0F, 0.0F);
            }

            const bool changed = next_tile != source_fluid_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] ||
                                 std::abs(next_amount - source_amounts[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) > 0.5F;
            if (changed) {
                PushChangedTile(changed_tiles, tile_coord);
            }
        }
    }

    stage.fluid_tiles = std::move(next_fluid_tiles);
    stage.fluid_amount = std::move(next_amounts);
    stage.fluid_velocity = std::move(next_velocities);
    if (!changed_tiles.empty()) {
        stage.tile_change_generation += 1;
        UpdateStageLightingForTileChanges(state, changed_tiles);
        UpdateStageAcousticsForTileChanges(state, changed_tiles);
    }
}

} // namespace splonks

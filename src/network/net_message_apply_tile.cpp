#include "network/net_message_apply_internal.hpp"

#include "audio_emitters.hpp"
#include "stage_acoustics.hpp"
#include "stage_break.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace splonks::network {

namespace {

bool IsSimulatedFluidTile(Tile tile) {
    return tile != Tile::Air && GetTileArchetype(tile).simulated_fluid;
}

} // namespace

void ApplyTileBrokenMessage(State& state, Audio* audio, const TileBrokenMessage& payload) {
    if (audio != nullptr) {
        BreakStageTilesAtCoords(
            {payload.tile_pos},
            state,
            *audio,
            std::nullopt,
            false,
            true,
            true,
            true
        );
        return;
    }

    const IVec2 tile_pos = state.stage.WrapTileCoord(payload.tile_pos);
    if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return;
    }
    (void)state.stage.TakeEmbeddedTreasure(tile_pos);
    state.stage.SetTile(tile_pos, Tile::Air);
    const std::vector<IVec2> changed_tiles{tile_pos};
    UpdateStageLightingForTileChanges(state, changed_tiles);
    UpdateStageAcousticsForTileChanges(state, changed_tiles);
}

void ApplyTileChangedMessage(State& state, const TileChangedMessage& payload) {
    const IVec2 wrapped_pos = state.stage.WrapTileCoord(payload.tile_pos);
    if (!state.stage.IsTileCoordInside(wrapped_pos.x, wrapped_pos.y)) {
        return;
    }

    if (payload.layer == NetTileLayer::Backwall) {
        const Tile current = state.stage.GetBackwallTile(
            static_cast<unsigned int>(wrapped_pos.x),
            static_cast<unsigned int>(wrapped_pos.y)
        );
        if (current == payload.tile) {
            return;
        }
        state.stage.SetBackwallTile(wrapped_pos, payload.tile);
    } else {
        const Tile current = state.stage.GetTile(
            static_cast<unsigned int>(wrapped_pos.x),
            static_cast<unsigned int>(wrapped_pos.y)
        );
        const TileRotation current_rotation = state.stage.GetTileRotation(
            static_cast<unsigned int>(wrapped_pos.x),
            static_cast<unsigned int>(wrapped_pos.y)
        );
        const TileRotation rotation = NormalizeTileRotation(payload.rotation);
        if (current == payload.tile && current_rotation == rotation) {
            return;
        }
        state.stage.SetTile(wrapped_pos, payload.tile);
        state.stage.SetTileRotation(wrapped_pos, rotation);
    }

    const std::vector<IVec2> changed_tiles{wrapped_pos};
    UpdateStageLightingForTileChanges(state, changed_tiles);
    UpdateStageAcousticsForTileChanges(state, changed_tiles);
}

void ApplyFluidCellPatchedMessage(State& state, const FluidCellPatchedMessage& payload) {
    const IVec2 wrapped_pos = state.stage.WrapTileCoord(payload.tile_pos);
    if (!state.stage.IsTileCoordInside(wrapped_pos.x, wrapped_pos.y)) {
        return;
    }

    Stage& stage = state.stage;
    stage.SyncTileInstanceMetadataGrid();
    const std::size_t y = static_cast<std::size_t>(wrapped_pos.y);
    const std::size_t x = static_cast<std::size_t>(wrapped_pos.x);
    const Tile current_tile = stage.fluid_tiles[y][x];
    const float current_amount = stage.fluid_amount[y][x];

    Tile next_tile = IsSimulatedFluidTile(payload.tile) ? payload.tile : Tile::Air;
    float next_amount = std::clamp(payload.amount, 0.0F, 1.0F);
    Vec2 next_velocity = payload.velocity;
    Vec2 next_temp_gravity = payload.temp_gravity;
    if (next_tile == Tile::Air || next_amount <= 0.0001F) {
        next_tile = Tile::Air;
        next_amount = 0.0F;
        next_velocity = Vec2::New(0.0F, 0.0F);
        next_temp_gravity = Vec2::New(0.0F, 0.0F);
    }

    const bool lighting_changed =
        current_tile != next_tile ||
        std::abs(current_amount - next_amount) > 0.0001F;
    stage.fluid_tiles[y][x] = next_tile;
    stage.fluid_amount[y][x] = next_amount;
    stage.fluid_display_amount[y][x] = next_amount;
    stage.fluid_velocity[y][x] = next_velocity;
    stage.fluid_gravity[y][x] = payload.gravity;
    stage.fluid_gravity_strength[y][x] = std::max(0.0F, payload.gravity_strength);
    stage.fluid_temp_gravity[y][x] = next_temp_gravity;

    if (lighting_changed) {
        const std::vector<IVec2> changed_tiles{wrapped_pos};
        UpdateStageLightingForTileChanges(state, changed_tiles);
        UpdateStageAcousticsForTileChanges(state, changed_tiles);
    }
}

} // namespace splonks::network

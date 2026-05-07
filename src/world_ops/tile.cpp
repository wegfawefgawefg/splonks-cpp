#include "world_ops.hpp"

#include "entity.hpp"
#include "network/net_gameplay_replication.hpp"
#include "network/net_session.hpp"
#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"

#include <vector>

namespace splonks::world_ops {

bool SetForegroundTile(
    State& state,
    const IVec2& tile_pos_raw,
    Tile tile,
    TileRotation rotation,
    bool allow_peer_canonical_apply
) {
    if (state.net_session.role == network::NetRole::Peer && !allow_peer_canonical_apply) {
        return false;
    }

    const IVec2 tile_pos = state.stage.WrapTileCoord(tile_pos_raw);
    if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return false;
    }

    const TileRotation normalized_rotation = NormalizeTileRotation(rotation);
    const Tile old_tile = state.stage.GetTile(
        static_cast<unsigned int>(tile_pos.x),
        static_cast<unsigned int>(tile_pos.y)
    );
    const TileRotation old_rotation = state.stage.GetTileRotation(
        static_cast<unsigned int>(tile_pos.x),
        static_cast<unsigned int>(tile_pos.y)
    );
    if (old_tile == tile && old_rotation == normalized_rotation) {
        return false;
    }

    state.stage.SetTile(tile_pos, tile);
    state.stage.SetTileRotation(tile_pos, normalized_rotation);

    const std::vector<IVec2> changed_tiles{tile_pos};
    UpdateStageLightingForTileChanges(state, changed_tiles);
    UpdateStageAcousticsForTileChanges(state, changed_tiles);

    network::ReplicateTileChanged(
        state,
        GameplayTileChanged{
            .tile_pos = tile_pos,
            .tile = tile,
            .rotation = normalized_rotation,
            .layer = GameplayTileLayer::Foreground,
        }
    );
    return true;
}

bool PlaceRopeTile(
    State& state,
    const Entity& source_entity,
    const IVec2& tile_pos_raw,
    bool allow_peer_canonical_apply
) {
    if (state.net_session.role == network::NetRole::Peer && !allow_peer_canonical_apply) {
        return false;
    }

    const IVec2 tile_pos = state.stage.WrapTileCoord(tile_pos_raw);
    if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
        return false;
    }

    const Tile old_tile = state.stage.GetTile(
        static_cast<unsigned int>(tile_pos.x),
        static_cast<unsigned int>(tile_pos.y)
    );
    if (old_tile == Tile::Rope) {
        return false;
    }

    state.stage.SetTile(tile_pos, Tile::Rope);
    network::ReplicateRopeTilePlaced(
        state,
        GameplayRopeTilePlaced{
            .source_vid = source_entity.vid,
            .tile_pos = tile_pos,
        }
    );
    return true;
}

void CommitTileBroken(State& state, const IVec2& tile_pos) {
    network::ReplicateTileBroken(
        state,
        GameplayTileBroken{
            .tile_pos = tile_pos,
        }
    );
}

} // namespace splonks::world_ops

#include "stage_break.hpp"

#include "ent/spec.hpp"
#include "on_damage_effects.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"
#include "stage_tile_triggers.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <array>

namespace splonks {

namespace {

Ent* SpawnEntAtCenter(EntType type_, FxVec2 center, State& state) {
    return world_ops::SpawnEnt(state, type_, [center](Ent& ent) {
        ent.SetCenter(center);
        ent.vel = FxVec2::zero();
    });
}

void SpawnEmbeddedTreasureDrops(const EmbeddedTreasure& embedded_treasure, const IVec2& tile_pos, State& state) {
    const FxVec2 center = FxVec2::from_int(
        tile_pos.x * static_cast<int>(kTileSize) + 8,
        tile_pos.y * static_cast<int>(kTileSize) + 8
    );
    static const std::array<FxVec2, 8> kDropOffsets{{
        FxVec2::from_int(-4, -1),
        FxVec2::from_int(0, 1),
        FxVec2::from_int(4, -1),
        FxVec2::from_int(-2, 3),
        FxVec2::from_int(2, -3),
        FxVec2::from_int(-5, 2),
        FxVec2::from_int(5, 2),
        FxVec2::from_int(0, -4),
    }};

    std::size_t offset_index = 0;
    for (const EmbeddedTreasureDrop& drop : embedded_treasure.drops) {
        if (drop.type_ == EntType::None || drop.count <= 0) {
            continue;
        }
        for (int i = 0; i < drop.count; ++i) {
            SpawnEntAtCenter(
                drop.type_,
                center + kDropOffsets[offset_index % kDropOffsets.size()],
                state
            );
            ++offset_index;
        }
    }
}

void SpawnTileBreakAnim(AFrameId anim_id, const IVec2& tile_pos, State& state) {
    const FVec2 center = FVec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
    SpawnDamageEffectAnimBurst(anim_id, center, state);
}

void NotifyAreaEntsTileChanged(const IVec2& tile_pos, State& state, Audio& audio) {
    const FxVec2 tile_center = FxVec2::from_int(
        tile_pos.x * static_cast<int>(kTileSize) + 8,
        tile_pos.y * static_cast<int>(kTileSize) + 8
    );
    const FxAABB tile_point_aabb = FxAABB::from_corners(tile_center, tile_center);

    for (const VID& vid : QueryEntsInAabb(state, tile_point_aabb)) {
        const Ent* const ent = state.ents.GetEnt(vid);
        if (ent == nullptr || !ent->active || ent->on_area_tile_changed == nullptr) {
            continue;
        }
        if (!WorldAabbContainsPoint(state.stage, ent->GetAABB(), tile_center)) {
            continue;
        }

        ent->on_area_tile_changed(vid.id, tile_pos, state, audio);
    }
}

void BreakStageTilesAtCoordsInternal(
    const std::vector<IVec2>& tile_positions,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound,
    bool suppress_tile_break_sound,
    std::optional<FVec2> sound_center,
    bool suppress_drop_spawns
) {
    std::optional<AudioAssetId> break_sound = std::nullopt;
    bool broke_any_tiles = false;
    std::vector<IVec2> changed_tiles;

    for (const IVec2& tile_pos_raw : tile_positions) {
        const IVec2 tile_pos = state.stage.WrapTileCoord(tile_pos_raw);
        if (!state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
            continue;
        }

        const Tile tile = state.stage.GetTile(static_cast<unsigned int>(tile_pos.x), static_cast<unsigned int>(tile_pos.y));
        if (tile == Tile::Air) {
            continue;
        }

        const TileSpec& tile_spec = GetTileSpec(tile);
        if (!suppress_tile_break_sound && !break_sound.has_value() && tile_spec.break_sound.has_value()) {
            break_sound = tile_spec.break_sound;
        }
        if (tile_spec.break_anim.has_value()) {
            SpawnTileBreakAnim(*tile_spec.break_anim, tile_pos, state);
        }
        if (!suppress_drop_spawns && tile_spec.on_break != nullptr) {
            tile_spec.on_break(tile_pos, state, audio);
        }
        RunStageTileDestroyedTriggers(tile_pos, state, audio);
        NotifyAreaEntsTileChanged(tile_pos, state, audio);

        const EmbeddedTreasure embedded_treasure = state.stage.TakeEmbeddedTreasure(tile_pos);
        if (!embedded_treasure.IsEmpty()) {
            if (embedded_treasure.break_sound != kInvalidAudioAssetId) {
                break_sound = embedded_treasure.break_sound;
            }
            if (!suppress_drop_spawns) {
                SpawnEmbeddedTreasureDrops(embedded_treasure, tile_pos, state);
            }
        }

        state.stage.SetTile(tile_pos, Tile::Air);
        changed_tiles.push_back(tile_pos);
        broke_any_tiles = true;
    }

    if (broke_any_tiles) {
        UpdateStageLightingForTileChanges(state, changed_tiles);
        UpdateStageAcousticsForTileChanges(state, changed_tiles);
    }
    const FVec2 emitter_center = sound_center.value_or(FVec2::New(0.0F, 0.0F));
    if (override_break_sound.has_value()) {
        (void)PlayWorldSoundEmitter(state, emitter_center, *override_break_sound);
    } else if (break_sound.has_value()) {
        (void)PlayWorldSoundEmitter(state, emitter_center, *break_sound);
    }
}

} // namespace

void BreakStageTilesInRectWc(
    FxAABB area,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound,
    bool suppress_tile_break_sound,
    bool suppress_drop_spawns
) {
    std::vector<IVec2> tile_positions;
    const std::vector<WorldTileQueryResult> tile_queries = QueryTilesInAabb(state.stage, area);
    tile_positions.reserve(tile_queries.size());
    for (const WorldTileQueryResult& tile_query : tile_queries) {
        if (tile_query.tile != nullptr) {
            tile_positions.push_back(tile_query.tile_pos);
        }
    }
    BreakStageTilesAtCoordsInternal(
        tile_positions,
        state,
        audio,
        override_break_sound,
        suppress_tile_break_sound,
        ToFVec2(area.center()),
        suppress_drop_spawns
    );
}

void BreakStageTilesAtCoords(
    const std::vector<IVec2>& tile_positions,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound,
    bool suppress_tile_break_sound,
    bool suppress_drop_spawns
) {
    FVec2 sound_center = FVec2::New(0.0F, 0.0F);
    if (!tile_positions.empty()) {
        for (const IVec2& tile_pos : tile_positions) {
            sound_center += FVec2::New(
                static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
                static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
            );
        }
        sound_center = sound_center / static_cast<float>(tile_positions.size());
    }
    BreakStageTilesAtCoordsInternal(
        tile_positions,
        state,
        audio,
        override_break_sound,
        suppress_tile_break_sound,
        sound_center,
        suppress_drop_spawns
    );
}

} // namespace splonks

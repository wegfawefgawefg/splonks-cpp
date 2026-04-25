#include "stage_break.hpp"

#include "entity/archetype.hpp"
#include "on_damage_effects.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"

#include <array>

namespace splonks {

namespace {

void SpawnEntityAtCenter(EntityType type_, const Vec2& center, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
}

void SpawnEmbeddedTreasureDrops(const EmbeddedTreasure& embedded_treasure, const IVec2& tile_pos, State& state) {
    const Vec2 center = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
    static const std::array<Vec2, 8> kDropOffsets{{
        Vec2::New(-4.0F, -1.0F),
        Vec2::New(0.0F, 1.0F),
        Vec2::New(4.0F, -1.0F),
        Vec2::New(-2.0F, 3.0F),
        Vec2::New(2.0F, -3.0F),
        Vec2::New(-5.0F, 2.0F),
        Vec2::New(5.0F, 2.0F),
        Vec2::New(0.0F, -4.0F),
    }};

    std::size_t offset_index = 0;
    for (const EmbeddedTreasureDrop& drop : embedded_treasure.drops) {
        if (drop.type_ == EntityType::None || drop.count <= 0) {
            continue;
        }
        for (int i = 0; i < drop.count; ++i) {
            SpawnEntityAtCenter(
                drop.type_,
                center + kDropOffsets[offset_index % kDropOffsets.size()],
                state
            );
            ++offset_index;
        }
    }
}

void SpawnTileBreakAnimation(FrameDataId animation_id, const IVec2& tile_pos, State& state) {
    const Vec2 center = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
    SpawnDamageEffectAnimationBurst(animation_id, center, state);
}

void NotifyAreaEntitiesTileChanged(const IVec2& tile_pos, State& state, Audio& audio) {
    const Vec2 tile_center = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
    const AABB tile_point_aabb = AABB::New(tile_center, tile_center);

    for (const VID& vid : QueryEntitiesInAabb(state, tile_point_aabb)) {
        const Entity* const entity = state.entity_manager.GetEntity(vid);
        if (entity == nullptr || !entity->active || entity->on_area_tile_changed == nullptr) {
            continue;
        }
        if (!WorldAabbContainsPoint(state.stage, entity->GetAABB(), tile_center)) {
            continue;
        }

        entity->on_area_tile_changed(vid.id, tile_pos, state, audio);
    }
}

} // namespace

namespace {

void BreakStageTilesAtCoordsInternal(
    const std::vector<IVec2>& tile_positions,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound,
    bool suppress_tile_break_sound,
    std::optional<Vec2> sound_center
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

        const TileArchetype& tile_archetype = GetTileArchetype(tile);
        if (!suppress_tile_break_sound && !break_sound.has_value() && tile_archetype.break_sound.has_value()) {
            break_sound = tile_archetype.break_sound;
        }
        if (tile_archetype.break_animation.has_value()) {
            SpawnTileBreakAnimation(*tile_archetype.break_animation, tile_pos, state);
        }
        if (tile_archetype.on_break != nullptr) {
            tile_archetype.on_break(tile_pos, state, audio);
        }
        NotifyAreaEntitiesTileChanged(tile_pos, state, audio);

        const EmbeddedTreasure embedded_treasure = state.stage.TakeEmbeddedTreasure(tile_pos);
        if (!embedded_treasure.IsEmpty()) {
            if (embedded_treasure.break_sound != kInvalidAudioAssetId) {
                break_sound = embedded_treasure.break_sound;
            }
            SpawnEmbeddedTreasureDrops(embedded_treasure, tile_pos, state);
        }

        state.stage.SetTile(tile_pos, Tile::Air);
        changed_tiles.push_back(tile_pos);
        broke_any_tiles = true;
    }

    if (broke_any_tiles) {
        UpdateStageLightingForTileChanges(state, changed_tiles);
        UpdateStageAcousticsForTileChanges(state, changed_tiles);
    }
    const Vec2 emitter_center = sound_center.value_or(Vec2::New(0.0F, 0.0F));
    if (override_break_sound.has_value()) {
        (void)PlayWorldSoundEmitter(state, emitter_center, *override_break_sound);
    } else if (break_sound.has_value()) {
        (void)PlayWorldSoundEmitter(state, emitter_center, *break_sound);
    }
}

} // namespace

void BreakStageTilesInRectWc(
    const AABB& area,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound,
    bool suppress_tile_break_sound
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
        (area.tl + area.br) / 2.0F
    );
}

void BreakStageTilesAtCoords(
    const std::vector<IVec2>& tile_positions,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound,
    bool suppress_tile_break_sound
) {
    Vec2 sound_center = Vec2::New(0.0F, 0.0F);
    if (!tile_positions.empty()) {
        for (const IVec2& tile_pos : tile_positions) {
            sound_center += Vec2::New(
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
        sound_center
    );
}

} // namespace splonks

#include "stage_break.hpp"

#include "entity/archetype.hpp"
#include "on_damage_effects.hpp"
#include "stage_lighting.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"

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

void BreakStageTilesInRectWc(const AABB& area, State& state, Audio& audio) {
    std::optional<SoundEffect> break_sound = std::nullopt;
    bool broke_any_tiles = false;
    std::vector<IVec2> changed_tiles;
    const std::vector<WorldTileQueryResult> tile_queries = QueryTilesInAabb(state.stage, area);

    for (const WorldTileQueryResult& tile_query : tile_queries) {
        if (tile_query.tile == nullptr) {
            continue;
        }

        const IVec2 tile_pos = tile_query.tile_pos;
        const Tile tile = *tile_query.tile;

        const TileArchetype& tile_archetype = GetTileArchetype(tile);
        if (!break_sound.has_value() && tile_archetype.break_sound.has_value()) {
            break_sound = tile_archetype.break_sound;
        }
        if (tile_archetype.break_animation.has_value()) {
            SpawnTileBreakAnimation(*tile_archetype.break_animation, tile_pos, state);
        }
        if (tile_archetype.on_break != nullptr) {
            tile_archetype.on_break(tile_pos, state, audio);
        }
        NotifyAreaEntitiesTileChanged(tile_pos, state, audio);

        const EntityType embedded_treasure = state.stage.TakeEmbeddedTreasure(tile_pos);
        if (embedded_treasure != EntityType::None) {
            const Vec2 center = Vec2::New(
                static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
                static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
            );
            SpawnEntityAtCenter(embedded_treasure, center, state);
        }

        state.stage.SetTile(tile_pos, Tile::Air);
        changed_tiles.push_back(tile_pos);
        broke_any_tiles = true;
    }

    if (broke_any_tiles) {
        UpdateStageLightingForTileChanges(state, changed_tiles);
    }
    if (break_sound.has_value()) {
        audio.PlaySoundEffect(*break_sound);
    }
}

} // namespace splonks

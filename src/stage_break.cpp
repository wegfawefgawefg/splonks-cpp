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

} // namespace

void BreakStageTilesInRectWc(const AABB& area, State& state, Audio& audio) {
    const IVec2 tl = ToIVec2(area.tl / static_cast<float>(kTileSize));
    const IVec2 br = ToIVec2(area.br / static_cast<float>(kTileSize));

    std::optional<SoundEffect> break_sound = std::nullopt;
    bool broke_any_tiles = false;
    const std::vector<IVec2> tile_positions = GetTileCoordsInRect(state.stage, tl, br);

    for (const IVec2& tile_pos : tile_positions) {
        const Tile tile = state.stage.GetTile(
            static_cast<unsigned int>(tile_pos.x),
            static_cast<unsigned int>(tile_pos.y)
        );
        if (tile == Tile::Exit) {
            continue;
        }

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

        const EntityType embedded_treasure = state.stage.TakeEmbeddedTreasure(tile_pos);
        if (embedded_treasure != EntityType::None) {
            const Vec2 center = Vec2::New(
                static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
                static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
            );
            SpawnEntityAtCenter(embedded_treasure, center, state);
        }

        state.stage.SetTile(tile_pos, Tile::Air);
        broke_any_tiles = true;
    }

    if (broke_any_tiles) {
        InvalidateStageLighting(state);
    }
    if (break_sound.has_value()) {
        audio.PlaySoundEffect(*break_sound);
    }
}

} // namespace splonks

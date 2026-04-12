#include "entities/rope.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "stage.hpp"
#include "state.hpp"
#include "render/terrain_lighting.hpp"
#include "tile.hpp"

namespace splonks::entities::rope {

extern const EntityArchetype kRopeArchetype{
    .type_ = EntityType::Rope,
    .size = Vec2::New(8.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .has_ground_friction = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .on_use = OnUseAsRope,
    .step_logic = StepEntityLogicAsRope,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::RopeBall),
};

void OnUseAsRope(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Entity& rope = state.entity_manager.entities[entity_idx];
    if (rope.use_state.pressed == false || rope.counter_a > 0.0F) {
        return;
    }

    rope.counter_a = 16.0F;
    SetAnimation(rope, frame_data_ids::UnfoldingRope);

    if (rope.use_state.source == AttachmentMode::None) {
        StopUsingEntity(rope);
    }
}

void StepEntityLogicAsRope(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    (void)graphics;
    Entity& rope = state.entity_manager.entities[entity_idx];

    // if rope is in winding up
    // set animation and display state
    // start decrementing the counter
    bool rope_popped = false;
    if (rope.counter_a > 0.0F) {
        rope.counter_a -= 1.0F;
        if (rope.counter_a <= 0.0F) {
            rope_popped = true;
            // pop
        }
    }
    constexpr unsigned int kRopeLength = 6;
    if (rope_popped) {
        rope.health = 0;
        // loop down up to 6 tiles, convert all air into rope tiles, but stop if interupped
        // get rope tile position,
        const Vec2 rope_center = rope.GetCenter();
        bool atleast_one_tile_converted = false;
        const IVec2 tile_pos = state.stage.GetTileCoordAtWc(ToIVec2(rope_center));
        if (state.stage.TileCoordAtWcExists(ToIVec2(rope_center))) {
            for (unsigned int y_offset = 0; y_offset < kRopeLength; ++y_offset) {
                IVec2 p = IVec2::New(tile_pos.x, tile_pos.y + static_cast<int>(y_offset));
                if (p.y >= static_cast<int>(state.stage.GetTileHeight())) {
                    p.y = static_cast<int>(state.stage.GetTileHeight()) - 1;
                }
                const Tile& tile = state.stage.GetTile(static_cast<unsigned int>(p.x), static_cast<unsigned int>(p.y));
                // if the tile is air, set it to rope
                if (tile == Tile::Air || tile == Tile::Rope || tile == Tile::Entrance) {
                    state.stage.SetTile(p, Tile::Rope);
                    graphics.ResetTileVariation(p);
                    UpdateTerrainLightingCacheForTileChange(state, p);
                    atleast_one_tile_converted = true;
                } else {
                    break;
                }
            }
        }

        if (atleast_one_tile_converted) {
            audio.PlaySoundEffect(SoundEffect::RopeDeploy);
        }
    }
}

/** generalize this to all square or rectangular entities somehow */
} // namespace splonks::entities::rope

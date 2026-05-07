#include "entities/rope.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "gameplay_authority.hpp"
#include "graphics.hpp"
#include "stage.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

namespace splonks::entities::rope {

namespace {

IVec2 GetRopeDeployStartTile(const Stage& stage, const IVec2& hit_tile_pos) {
    const std::optional<WorldTileQueryResult> hit_tile =
        QueryTileAtTilePos(stage, hit_tile_pos);
    if (!hit_tile.has_value() || !IsTileQueryClimbable(stage, *hit_tile)) {
        return hit_tile_pos;
    }

    IVec2 start = hit_tile->tile_pos;
    while (true) {
        const IVec2 next = IVec2::New(start.x, start.y + 1);
        const std::optional<WorldTileQueryResult> next_tile =
            QueryTileAtTilePos(stage, next);
        if (!next_tile.has_value() || !IsTileQueryClimbable(stage, *next_tile)) {
            return next;
        }
        start = next_tile->tile_pos;
    }
}

} // namespace

extern const EntityArchetype kRopeArchetype{
    .type_ = EntityType::Rope,
    .size = Vec2::New(8.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .step_as_replica = true,
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
    (void)audio;
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
        if (!HasLocalGameplayAuthorityForEntity(state, rope.vid)) {
            state.entity_manager.SetInactive(entity_idx);
            return;
        }
        rope.health = 0;
        // loop down up to 6 tiles, convert all air into rope tiles, but stop if interupped
        // get rope tile position,
        const Vec2 rope_center = rope.GetCenter();
        bool atleast_one_tile_converted = false;
        const std::optional<WorldTileQueryResult> rope_tile =
            QueryTileAtWorldPos(state.stage, ToIVec2(rope_center));
        if (rope_tile.has_value()) {
            const IVec2 deploy_start_tile =
                GetRopeDeployStartTile(state.stage, rope_tile->tile_pos);
            for (unsigned int y_offset = 0; y_offset < kRopeLength; ++y_offset) {
                const std::optional<WorldTileQueryResult> tile_query = QueryTileAtTilePos(
                    state.stage,
                    IVec2::New(deploy_start_tile.x, deploy_start_tile.y + static_cast<int>(y_offset))
                );
                if (!tile_query.has_value() || tile_query->tile == nullptr) {
                    break;
                }

                const IVec2 p = tile_query->tile_pos;
                const Tile tile = *tile_query->tile;
                // if the tile is air, set it to rope
                if (tile == Tile::Air || tile == Tile::Rope || tile == Tile::Entrance) {
                    if (world_ops::PlaceRopeTile(state, rope, p)) {
                        graphics.ResetTileVariation(p);
                        atleast_one_tile_converted = true;
                    }
                } else {
                    break;
                }
            }
        }

        if (atleast_one_tile_converted) {
            (void)PlayEntityCenterSoundEmitter(state, state.entity_manager.entities[entity_idx], audio_asset_ids::RopeDeploy);
        }
        (void)world_ops::DeactivateEntity(state, rope.vid);
    }
}

/** generalize this to all square or rectangular entities somehow */
} // namespace splonks::entities::rope

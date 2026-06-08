#include "ents/rope.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "stage.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

namespace splonks::ents::rope {

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

extern const EntSpec kRopeSpec{
    .type_ = EntType::Rope,
    .size = EntSpecSize(8.0F, 6.0F),
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
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .on_use = OnUseAsRope,
    .step_logic = StepEntLogicAsRope,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::RopeBall),
};

void OnUseAsRope(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    (void)audio;
    Ent& rope = state.ents.ents[ent_idx];
    if (rope.use_state.pressed == false || rope.counter_a > sim::Scalar::zero()) {
        return;
    }

    rope.counter_a = sim::Scalar::from_int(16);
    SetAnim(rope, aframe_ids::UnfoldingRope);

    if (rope.use_state.source == AttachMode::None) {
        StopUsingEnt(rope);
    }
}

void StepEntLogicAsRope(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    (void)graphics;
    (void)audio;
    Ent& rope = state.ents.ents[ent_idx];

    // if rope is in winding up
    // set anim and display state
    // start decrementing the counter
    bool rope_popped = false;
    if (rope.counter_a > sim::Scalar::zero()) {
        rope.counter_a -= sim::Scalar::from_int(1);
        if (rope.counter_a <= sim::Scalar::zero()) {
            rope_popped = true;
            // pop
        }
    }
    constexpr std::uint32_t kRopeLength = 6;
    if (rope_popped) {
        rope.health = 0;
        // loop down up to 6 tiles, convert all air into rope tiles, but stop if interupped
        // get rope tile position,
        const Vec2 rope_center = rope.GetRenderCenter();
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
            (void)PlayEntCenterSoundEmitter(state, state.ents.ents[ent_idx], audio_asset_ids::RopeDeploy);
        }
        (void)world_ops::DeactivateEnt(state, rope.vid);
    }
}

/** generalize this to all square or rectangular ents somehow */
} // namespace splonks::ents::rope

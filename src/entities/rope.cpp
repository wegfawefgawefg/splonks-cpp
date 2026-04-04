#include "entities/rope.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "graphics.hpp"
#include "sprite.hpp"
#include "stage.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks::entities::rope {

void SetEntityRope(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::Rope;
    entity.size = Vec2::New(8.0F, 6.0F);
    entity.health = 1;
    entity.sprite_animator.SetSprite(Sprite::Rope);

    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.impassable = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
}

void StepEntityLogicAsRope(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    Graphics& graphics
) {
    Entity& rope = state.entity_manager.entities[entity_idx];

    // if rope is in use, initialize its timer, and set state to winding up
    if (rope.state == EntityState::InUse) {
        rope.counter_a = 16.0F;
        rope.state = EntityState::WindingUp;
        rope.display_state = EntityDisplayState::Neutral;
        rope.sprite_animator.SetSprite(Sprite::RopeWindingUp);
    }

    // if rope is in winding up
    // set animation and display state
    // start decrementing the counter
    bool rope_popped = false;
    if (rope.state == EntityState::WindingUp) {
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
                if (p.y >= static_cast<int>(Stage::kShape.y)) {
                    p.y = static_cast<int>(Stage::kShape.y) - 1;
                }
                const Tile& tile = state.stage.GetTile(static_cast<unsigned int>(p.x), static_cast<unsigned int>(p.y));
                // if the tile is air, set it to rope
                if (tile == Tile::Air || tile == Tile::Rope || tile == Tile::Entrance) {
                    state.stage.SetTile(p, Tile::Rope);
                    graphics.ResetTileVariation(p);
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
void StepEntityPhysicsAsRope(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::rope

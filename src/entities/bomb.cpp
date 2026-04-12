#include "entities/bomb.hpp"

#include "audio.hpp"
#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

namespace splonks::entities::bomb {

extern const EntityArchetype kBombArchetype{
    .type_ = EntityType::Bomb,
    .size = Vec2::New(8.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .super_state = EntitySuperState::Idle,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingAndSpikes,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Grenade),
};

void StepEntityLogicAsBomb(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& bomb = state.entity_manager.entities[entity_idx];
    const Vec2 bomb_center = bomb.GetCenter();
    if (bomb.super_state == EntitySuperState::Dead) {
        const Vec2 center = bomb_center;
        common::DoExplosion(entity_idx, center, 2.0F, state, audio);
        return;
    }

    // if bomb is in use, initialize its timer, and set state to winding up
    if (bomb.state == EntityState::InUse) {
        bomb.counter_a = 144.0F;
        bomb.state = EntityState::WindingUp;
        TrySetAnimation(bomb, EntityDisplayState::Stunned);
        SetAnimation(bomb, frame_data_ids::LiveGrenade);
    }

    // if bomb is in winding up
    // set animation and display state
    // start decrementing the counter
    if (bomb.state == EntityState::WindingUp) {
        bomb.counter_a -= 1.0F;
        if (bomb.counter_a <= 0.0F) {
            // do explosion
            const Vec2 center = bomb.GetCenter();
            common::DoExplosion(entity_idx, center, 2.0F, state, audio);
            bomb.health = 0;
        }
    }
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsBomb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::bomb

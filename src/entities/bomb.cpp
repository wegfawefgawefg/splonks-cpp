#include "entities/bomb.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "sprite.hpp"
#include "state.hpp"

namespace splonks::entities::bomb {

void SetEntityBomb(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::Bomb;
    entity.display_state = EntityDisplayState::Neutral;
    entity.size = Vec2::New(8.0F, 6.0F);
    entity.health = 1;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.impassable = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.damage_vulnerability = DamageVulnerability::CrushingAndSpikes;
    entity.alignment = Alignment::Neutral;
}

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
        bomb.display_state = EntityDisplayState::Stunned;
        bomb.sprite_animator.SetSprite(Sprite::BombTicking);
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
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::bomb

#include "entities/rock.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "sprite.hpp"
#include "state.hpp"

namespace splonks::entities::rock {

void SetEntityRock(Entity& entity) {
    entity.Reset();
    entity.health = 1;
    entity.active = true;
    entity.type_ = EntityType::Rock;
    entity.size = Vec2::New(6.0F, 5.0F);
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_picked_up = true;
    entity.damage_vulnerability = DamageVulnerability::CrushingOnly;
    entity.hurt_on_contact = false;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Projectile;
    entity.display_state = EntityDisplayState::Neutral;
    entity.facing = LeftOrRight::Left;
    entity.impassable = false;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    entity.sprite_animator.SetSprite(Sprite::Rock);
}

/** Rock does nothing, but if it hits an entity it should do rock damage and try to stun probs.
 * It should be a little bit bouncier than normal entities, also,
 * clunky sound on bounces, smack sound on hit something?
 * (do we need some material smack sounds: flesh, metal, bang, stone)
 * if grounded and moving, roll?? so set rotation
 */
void StepEntityLogicAsRock(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
    //TODO: if you hit the ground, do a clunky sound
    // if you hit something, do rock damage and try to stun probs
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsRock(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, audio);
    common::ApplyGroundFriction(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::rock

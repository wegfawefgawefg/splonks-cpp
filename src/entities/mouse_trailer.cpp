#include "entities/mouse_trailer.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks::entities::mouse_trailer {

void SetEntityMouseTrailer(Entity& entity) {
    entity.Reset();
    entity.active = true;
    entity.type_ = EntityType::MouseTrailer;
    entity.size = Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize));
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_picked_up = false;
    entity.impassable = false;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Projectile;
    TrySetDisplayState(entity, EntityDisplayState::Neutral);
    entity.facing = LeftOrRight::Left;
    entity.alignment = Alignment::Neutral;
    entity.frame_data_animator.SetAnimation(frame_data_ids::NoSprite);
}

/** mouse_trailer does nothing, if falling, it should instakill if it hits an entity, and that entity is also grounded.
 * It should be a little bit bouncier than normal entities, also,
 * clunky sound on bounces, smack sound on hit something?
 * (do we need some material smack sounds: flesh, metal, bang, stone)
 * if grounded and moving, roll?? so set rotation
 */
void StepEntityLogicAsMouseTrailer(
    std::size_t entity_idx,
    State& state,
    Audio& audio
) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsMouseTrailer(std::size_t entity_idx, State& state, float dt) {
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileCollisions(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::mouse_trailer

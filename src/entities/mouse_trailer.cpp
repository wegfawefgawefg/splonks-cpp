#include "entities/mouse_trailer.hpp"

#include "audio.hpp"
#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks::entities::mouse_trailer {

extern const EntityArchetype kMouseTrailerArchetype{
    .type_ = EntityType::MouseTrailer,
    .size = Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .super_state = EntitySuperState::Idle,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::NoSprite),
};

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

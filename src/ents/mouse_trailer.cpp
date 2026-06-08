#include "ents/mouse_trailer.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks::ents::mouse_trailer {

extern const EntSpec kMouseTrailerSpec{
    .type_ = EntType::MouseTrailer,
    .size = EntSpecSize(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .render_enabled = false,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .step_physics = StepEntPhysicsAsMouseTrailer,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::NoSprite),
};

/** mouse_trailer does nothing, if falling, it should instakill if it hits an ent, and that ent is also grounded.
 * It should be a little bit bouncier than normal ents, also,
 * clunky sound on bounces, smack sound on hit something?
 * (do we need some material smack sounds: flesh, metal, bang, stone)
 * if grounded and moving, roll?? so set rotation
 *//** generalize this to all square or rectangular ents somehow */
void StepEntPhysicsAsMouseTrailer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    common::PrePartialEulerStep(ent_idx, state, dt);
    common::DoTileCollisions(ent_idx, state);
    common::PostPartialEulerStep(ent_idx, state, dt);
}

} // namespace splonks::ents::mouse_trailer

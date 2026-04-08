#include "entities/rock.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "systems/controls.hpp"

namespace splonks::entities::rock {

namespace {

constexpr float kControlledMoveAcc = 0.12F;
constexpr float kControlledAirMoveAcc = 0.05F;
constexpr float kControlledJumpVel = 4.0F;
constexpr float kControlledSlideVel = 4.5F;
constexpr std::uint32_t kControlledSlideCooldownFrames = 90;

bool IsControlled(const Entity& entity, const State& state) {
    return state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid;
}

void StepControlledRock(Entity& rock, const systems::controls::ControlIntent& control) {
    if (rock.attack_delay_countdown > 0) {
        rock.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        rock.acc.x -= rock.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        rock.facing = LeftOrRight::Left;
    } else if (control.right && !control.left) {
        rock.acc.x += rock.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        rock.facing = LeftOrRight::Right;
    }

    if (control.jump_pressed && rock.grounded) {
        rock.vel.y = -kControlledJumpVel;
        rock.grounded = false;
    }

    if (control.attack_pressed && rock.grounded && rock.attack_delay_countdown == 0) {
        const float slide_vel = rock.facing == LeftOrRight::Left ? -kControlledSlideVel
                                                                 : kControlledSlideVel;
        rock.vel.x = slide_vel;
        rock.attack_delay_countdown = kControlledSlideCooldownFrames;
    }
}

} // namespace

void SetEntityRock(Entity& entity) {
    entity.Reset();
    entity.health = 1;
    entity.active = true;
    entity.type_ = EntityType::Rock;
    entity.size = Vec2::New(6.0F, 5.0F);
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_picked_up = true;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.hurt_on_contact = false;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Projectile;
    entity.display_state = EntityDisplayState::Neutral;
    entity.facing = LeftOrRight::Left;
    entity.impassable = false;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    entity.frame_data_animator.SetAnimation(frame_data_ids::Rock);
}

/** Rock does nothing, but if it hits an entity it should do rock damage and try to stun probs.
 * It should be a little bit bouncier than normal entities, also,
 * clunky sound on bounces, smack sound on hit something?
 * (do we need some material smack sounds: flesh, metal, bang, stone)
 * if grounded and moving, roll?? so set rotation
 */
void StepEntityLogicAsRock(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& rock = state.entity_manager.entities[entity_idx];
    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(rock, state);
    if (IsControlled(rock, state) && rock.super_state != EntitySuperState::Dead &&
        rock.super_state != EntitySuperState::Crushed) {
        StepControlledRock(rock, control);
    }
    (void)audio;
    //TODO: if you hit the ground, do a clunky sound
    // if you hit something, do rock damage and try to stun probs
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsRock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::ApplyGroundFriction(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::rock

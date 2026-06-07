#include "ents/rock.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "controls.hpp"

namespace splonks::ents::rock {

namespace {

constexpr float kControlledMoveAcc = 0.12F;
constexpr float kControlledAirMoveAcc = 0.05F;
constexpr float kControlledJumpVel = 4.0F;
constexpr float kControlledSlideVel = 4.5F;
constexpr std::uint32_t kControlledSlideCooldownFrames = 90;


void StepControlledRock(Ent& rock, const controls::ControlIntent& control) {
    if (rock.attack_delay_countdown > 0) {
        rock.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        rock.acc.x -= rock.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        rock.facing = Side::Left;
    } else if (control.right && !control.left) {
        rock.acc.x += rock.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        rock.facing = Side::Right;
    }

    if (control.jump_pressed && rock.grounded) {
        rock.vel.y = -kControlledJumpVel;
        rock.grounded = false;
    }

    if (control.attack_pressed && rock.grounded && rock.attack_delay_countdown == 0) {
        const float slide_vel = rock.facing == Side::Left ? -kControlledSlideVel
                                                                 : kControlledSlideVel;
        rock.vel.x = slide_vel;
        rock.attack_delay_countdown = kControlledSlideCooldownFrames;
    }
}

void ControlEntAsRock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& rock = state.ents.ents[ent_idx];
    if (rock.condition == EntCondition::Dead) {
        return;
    }

    StepControlledRock(rock, controls::GetControlIntentForEnt(rock, state));
}

} // namespace

extern const EntSpec kRockSpec{
    .type_ = EntType::Rock,
    .size = Vec2::New(6.0F, 5.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .buoyancy = sim::ToSimScalar(0.0F),
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .collide_sound = audio_asset_ids::Thud,
    .control_logic = ControlEntAsRock,
    .step_logic = StepEntLogicAsRock,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Rock),
};

/** Rock does nothing, but if it hits an ent it should do rock damage and try to stun probs.
 * It should be a little bit bouncier than normal ents, also,
 * clunky sound on bounces, smack sound on hit something?
 * (do we need some material smack sounds: flesh, metal, bang, stone)
 * if grounded and moving, roll?? so set rotation
 */
void StepEntLogicAsRock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)ent_idx;
    (void)state;
    (void)graphics;
    (void)audio;
    (void)dt;
    //TODO: if you hit the ground, do a clunky sound
    // if you hit something, do rock damage and try to stun probs
}

/** generalize this to all square or rectangular ents somehow */
} // namespace splonks::ents::rock

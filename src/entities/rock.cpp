#include "entities/rock.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "controls.hpp"

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

void StepControlledRock(Entity& rock, const controls::ControlIntent& control) {
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

extern const EntityArchetype kRockArchetype{
    .type_ = EntityType::Rock,
    .size = Vec2::New(6.0F, 5.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .collide_sound = SoundEffect::Thud,
    .step_logic = StepEntityLogicAsRock,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Rock),
};

/** Rock does nothing, but if it hits an entity it should do rock damage and try to stun probs.
 * It should be a little bit bouncier than normal entities, also,
 * clunky sound on bounces, smack sound on hit something?
 * (do we need some material smack sounds: flesh, metal, bang, stone)
 * if grounded and moving, roll?? so set rotation
 */
void StepEntityLogicAsRock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& rock = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(rock, state);
    if (IsControlled(rock, state) && rock.condition != EntityCondition::Dead) {
        StepControlledRock(rock, control);
    }
    (void)audio;
    //TODO: if you hit the ground, do a clunky sound
    // if you hit something, do rock damage and try to stun probs
}

/** generalize this to all square or rectangular entities somehow */
} // namespace splonks::entities::rock

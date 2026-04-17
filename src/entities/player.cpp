#include "entities/player.hpp"
#include "audio.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/block.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "controls.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace splonks::entities::player {

extern const EntityArchetype kPlayerArchetype{
    .type_ = EntityType::Player,
    .size = Vec2::New(10.0F, 10.0F),
    .health = 400,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_stomp = true,
    .can_hang_ledge = true,
    .can_be_stunned = true,
    .stun_recovers_on_ground = true,
    .stun_recovers_while_held = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .bombs = 400,
    .ropes = 400,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = SoundEffect::PlayerOuch,
    .step_logic = StepEntityLogicAsPlayer,
    .step_physics = StepEntityPhysicsAsPlayer,
    .alignment = Alignment::Ally,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::PlayerStanding),
};

void StepEntityLogicAsPlayer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    {
        // SKIP CONDITIONS
        Entity& player = state.entity_manager.entities[entity_idx];
        const EntityCondition player_condition = player.condition;
        const std::optional<VID> player_holding_vid = player.holding_vid;
        const std::optional<VID> player_back_vid = player.back_vid;
        if (player_condition == EntityCondition::Dead) {
            // if you are holding something, unhold it
            if (player_holding_vid.has_value()) {
                if (Entity* const holding = state.entity_manager.GetEntityMut(*player_holding_vid)) {
                    holding->held_by_vid.reset();
                    holding->attachment_mode = AttachmentMode::None;
                    StopUsingEntity(*holding);
                    holding->has_physics = true;
                    holding->can_collide = true;
                }
                player.holding_vid.reset();
            }
            // backpack release
            if (player_back_vid.has_value()) {
                if (Entity* const back = state.entity_manager.GetEntityMut(*player_back_vid)) {
                    back->held_by_vid.reset();
                    back->attachment_mode = AttachmentMode::None;
                    StopUsingEntity(*back);
                    back->has_physics = true;
                    back->can_collide = true;
                }
                player.back_vid.reset();
            }

            return;
        }
    }

    //  REQUIRED HACK FOR JETPACK TO CATCH GROUND TOUCH ON FRAME JUMP
    {
        Entity& player = state.entity_manager.entities[entity_idx];
        player.jumped_this_frame = false;
    }

    // TODO: probably put a check for dead or stunned up here lol
    common::StepTravelSoundWalkerClimber(entity_idx, state, audio);
    common::CleanupInactiveCarryReferences(entity_idx, state);

    const bool loss_of_control =
        state.entity_manager.entities[entity_idx].condition == EntityCondition::Stunned;
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(
            state.entity_manager.entities[entity_idx],
            state
        );

    {
        Entity& player = state.entity_manager.entities[entity_idx];
        const bool hanging = player.IsHanging();
        const bool climbing = player.IsClimbing();
        const bool walking =
            !loss_of_control &&
            (control.left != control.right) &&
            player.grounded &&
            !climbing &&
            !hanging;
        SetMovementFlag(player, EntityMovementFlag::Walking, walking);
        SetMovementFlag(player, EntityMovementFlag::Running, walking && control.run);
        SetMovementFlag(player, EntityMovementFlag::Climbing, climbing);
        SetMovementFlag(player, EntityMovementFlag::Hanging, hanging);
    }

    // SET ANIMATIONS AND DISPLAY STATES
    {
        Entity& player = state.entity_manager.entities[entity_idx];
        // if player moving left, set that
        if (player.vel.x < 0.0F) {
            player.facing = LeftOrRight::Left;
        }
        if (player.vel.x > 0.0F) {
            player.facing = LeftOrRight::Right;
        }

        // skip all actions
        if (!loss_of_control) {
            if (Length(player.vel) < 1.0F) {
                TrySetAnimation(player, EntityDisplayState::Neutral);
                if (player.holding_vid.has_value() || HasMovementFlag(player, EntityMovementFlag::Pushing)) {
                    TrySetAnimation(player, EntityDisplayState::NeutralHolding);
                }
            } else if (Length(player.vel) > 1.0F) {
                TrySetAnimation(player, EntityDisplayState::Walk);
                if (player.holding_vid.has_value() || HasMovementFlag(player, EntityMovementFlag::Pushing)) {
                    TrySetAnimation(player, EntityDisplayState::WalkHolding);
                }
            }
            // TODO: variable locomotion animation speed should be a common system
            // if player.vel.length() > 3.5 {
            //     player.animation_speed = DEFAULT_ANIMATION_SPEED / 2.0;
            // } else {
            //     player.animation_speed = DEFAULT_ANIMATION_SPEED;
            // }
            // if player hanging left set hanging display and left
            // if player hanging right set hanging display state and right

            if (player.hang_side == LeftOrRight::Left) {
                TrySetAnimation(player, EntityDisplayState::Hanging);
                player.facing = LeftOrRight::Left;
            } else if (player.hang_side == LeftOrRight::Right) {
                TrySetAnimation(player, EntityDisplayState::Hanging);
                player.facing = LeftOrRight::Right;
            } else if (player.IsClimbing()) {
                TrySetAnimation(player, EntityDisplayState::Climbing);
            }
            if (player.vel.y > 2.0F && !player.IsClimbing()) {
                // TODO: make an actual fall state
                TrySetAnimation(player, EntityDisplayState::Falling);
            }
        }
    }

    common::UpdateCarryAndBackItems(entity_idx, state, graphics, audio);

    // PLAYER TOOL SLOT 1
    if (!loss_of_control) {
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 0, control.bomb_pressed);
    }

    // PLAYER TOOL SLOT 2
    if (!loss_of_control) {
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 1, control.rope_pressed);
    }

    // PLAYER SWING BAT SECTION
    {
        Entity& player = state.entity_manager.entities[entity_idx];
        if (player.attack_delay_countdown > 0) {
            player.attack_delay_countdown -= 1;
        }
    }
    if (!loss_of_control) {
        const Entity& player = state.entity_manager.entities[entity_idx];
        const Vec2 player_pos = player.pos;
        const bool trying_to_attack = control.attack_pressed;
        const VID player_vid = player.vid;
        const unsigned int attack_delay_countdown = player.attack_delay_countdown;
        const bool has_held_item = player.holding_vid.has_value();

        bool attacked = false;
        if (trying_to_attack && attack_delay_countdown == 0 && !has_held_item) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const entity = state.entity_manager.GetEntityMut(*vid)) {
                    SetEntityAs(*entity, EntityType::BaseballBat);
                    entity->pos = player_pos;
                    entity->held_by_vid = player_vid;
                    entity->attachment_mode = AttachmentMode::Held;
                    state.UpdateSidForEntity(vid->id, graphics);
                    attacked = true;
                    audio.PlaySoundEffect(SoundEffect::BaseballBatSwing);
                }
            }
        }
        if (attacked) {
            Entity& mutable_player = state.entity_manager.entities[entity_idx];
            mutable_player.attack_delay_countdown = kAttackDelay;
        }
    }

    // PUSH BLOCKS
    if (!loss_of_control) {
        common::TryPushBlocks(entity_idx, state, graphics);
    }

}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsPlayer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::HangHandsStep(entity_idx, state);
    common::JumpingAndClimbingStep(entity_idx, state, audio);

    // custom pre partial euler step for player to apply special velocity clamping.
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.vel += entity.acc;
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(entity, state);
    if (control.run) {
        entity.vel.x = std::clamp(entity.vel.x, -kMaxRunSpeed, kMaxRunSpeed);
    } else {
        entity.vel.x = std::clamp(entity.vel.x, -kMaxWalkSpeed, kMaxWalkSpeed);
    }
    entity.vel.y = std::clamp(entity.vel.y, -kMaxSpeed, kMaxSpeed);

    if (!entity.IsHorizontallyControlled() && !entity.grounded) {
        entity.vel.x *= 0.85F;
    }
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::ApplyArchetypeGroundFriction(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::player

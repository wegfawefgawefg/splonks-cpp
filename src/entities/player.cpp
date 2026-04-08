#include "entities/player.hpp"

#include "audio.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/block.hpp"
#include "entities/bomb.hpp"
#include "entities/common.hpp"
#include "entities/rope.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "systems/controls.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace splonks::entities::player {

void SetEntityPlayer(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::Player;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    entity.pos = Vec2::New(0.0F, 0.0F);
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);
    entity.damage_vulnerability = DamageVulnerability::Vulnerable;
    entity.health = 400;
    entity.bombs = 400;
    entity.ropes = 400;
    entity.size = Vec2::New(10.0F, 10.0F);
    entity.has_physics = true;
    entity.can_collide = true;
    entity.impassable = false;
    entity.can_hang_ledge = true;
    entity.draw_layer = DrawLayer::Middle;
    entity.can_be_stunned = true;
    entity.alignment = Alignment::Ally;
    entity.hurt_on_contact = false;
    entity.frame_data_animator.SetAnimation(frame_data_ids::PlayerStanding);
}

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
        const EntitySuperState player_super_state = player.super_state;
        const std::optional<VID> player_holding_vid = player.holding_vid;
        const std::optional<VID> player_back_vid = player.back_vid;
        if (player_super_state == EntitySuperState::Dead) {
            // if you are holding something, unhold it
            if (player_holding_vid.has_value()) {
                if (Entity* const holding = state.entity_manager.GetEntityMut(*player_holding_vid)) {
                    holding->held_by_vid.reset();
                    holding->has_physics = true;
                    holding->can_collide = true;
                }
                player.holding_vid.reset();
            }
            // backpack release
            if (player_back_vid.has_value()) {
                if (Entity* const back = state.entity_manager.GetEntityMut(*player_back_vid)) {
                    back->held_by_vid.reset();
                    back->has_physics = true;
                    back->can_collide = true;
                }
                player.back_vid.reset();
            }

            return;
        }
        if (player.super_state == EntitySuperState::Crushed) {
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

    const bool loss_of_control =
        state.entity_manager.entities[entity_idx].super_state == EntitySuperState::Stunned;
    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(
            state.entity_manager.entities[entity_idx],
            state
        );

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
                player.display_state = EntityDisplayState::Neutral;
                if (player.holding_vid.has_value() || player.state == EntityState::Pushing) {
                    player.display_state = EntityDisplayState::NeutralHolding;
                }
            } else if (Length(player.vel) > 1.0F) {
                player.display_state = EntityDisplayState::Walk;
                if (player.holding_vid.has_value() || player.state == EntityState::Pushing) {
                    player.display_state = EntityDisplayState::WalkHolding;
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

            if (player.left_hanging) {
                player.display_state = EntityDisplayState::Hanging;
                player.facing = LeftOrRight::Left;
            } else if (player.right_hanging) {
                player.display_state = EntityDisplayState::Hanging;
                player.facing = LeftOrRight::Right;
            } else if (player.climbing) {
                player.display_state = EntityDisplayState::Climbing;
            }
            if (player.vel.y > 2.0F && !player.climbing) {
                // TODO: make an actual fall state
                player.display_state = EntityDisplayState::Falling;
            }
        }
    }

    common::UpdateCarryAndBackItems(entity_idx, state, graphics, audio);

    // PLAYER BOMB SECTION
    if (!loss_of_control) {
        {
            Entity& player = state.entity_manager.entities[entity_idx];
            if (player.bomb_throw_delay_countdown > 0) {
                player.bomb_throw_delay_countdown -= 1;
            }
        }

        const Entity& player = state.entity_manager.entities[entity_idx];
        const bool trying_to_bomb = control.bomb_pressed;
        const bool trying_to_go_up = control.up;
        const bool trying_to_go_down = control.down;
        const bool trying_to_go_left = control.left;
        const bool trying_to_go_right = control.right;
        const Vec2 player_center = player.GetCenter();
        const unsigned int bomb_count = player.bombs;
        const unsigned int bomb_throw_delay = player.bomb_throw_delay_countdown;
        const VID player_vid = player.vid;

        bool used_bomb = false;
        if (trying_to_bomb && bomb_count > 0 && bomb_throw_delay == 0) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const bomb = state.entity_manager.GetEntityMut(*vid)) {
                    bomb::SetEntityBomb(*bomb);
                    bomb->has_physics = true;
                    bomb->can_collide = true;
                    bomb->state = EntityState::InUse;
                    bomb->thrown_by = player_vid;
                    bomb->thrown_immunity_timer = common::kThrownByImmunityDuration;

                    Vec2 throw_vel = Vec2::New(0.0F, 0.0F);
                    if (trying_to_go_left) {
                        throw_vel.x = -10.0F;
                    } else if (trying_to_go_right) {
                        throw_vel.x = 10.0F;
                    }
                    if (trying_to_go_up) {
                        throw_vel.y = -10.0F;
                    }
                    if (trying_to_go_down) {
                        throw_vel.y = 10.0F;
                    }
                    if (!trying_to_go_up && !trying_to_go_down &&
                        (trying_to_go_left || trying_to_go_right)) {
                        throw_vel.y = -2.0F;
                    }
                    bomb->SetCenter(player_center);
                    bomb->acc += throw_vel;
                    audio.PlaySoundEffect(SoundEffect::Throw);
                    used_bomb = true;
                }
            }
        }

        if (used_bomb) {
            Entity& mutable_player = state.entity_manager.entities[entity_idx];
            if (mutable_player.bombs > 0) {
                mutable_player.bombs -= 1;
            }
            mutable_player.bomb_throw_delay_countdown = kBombThrowDelay;
        }
    }

    // PLAYER ROPE SECTION
    if (!loss_of_control) {
        {
            Entity& player = state.entity_manager.entities[entity_idx];
            if (player.rope_throw_delay_countdown > 0) {
                player.rope_throw_delay_countdown -= 1;
            }
        }

        const Entity& player = state.entity_manager.entities[entity_idx];
        const bool trying_to_rope = control.rope_pressed;
        const bool trying_to_go_up = control.up;
        const bool trying_to_go_down = control.down;
        const bool trying_to_go_left = control.left;
        const bool trying_to_go_right = control.right;
        const Vec2 player_center = player.GetCenter();
        const unsigned int rope_count = player.ropes;
        const unsigned int rope_throw_delay = player.rope_throw_delay_countdown;
        const VID player_vid = player.vid;

        bool used_rope = false;
        if (trying_to_rope && rope_count > 0 && rope_throw_delay == 0) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const rope_entity = state.entity_manager.GetEntityMut(*vid)) {
                    rope::SetEntityRope(*rope_entity);
                    rope_entity->has_physics = true;
                    rope_entity->can_collide = true;
                    rope_entity->state = EntityState::InUse;
                    rope_entity->thrown_by = player_vid;
                    rope_entity->thrown_immunity_timer = common::kThrownByImmunityDuration * 2;

                    Vec2 throw_vel = Vec2::New(0.0F, 0.0F);
                    if (trying_to_go_left) {
                        throw_vel.x = -10.0F;
                    } else if (trying_to_go_right) {
                        throw_vel.x = 10.0F;
                    }
                    if (trying_to_go_up) {
                        throw_vel.y = -10.0F;
                    }
                    if (trying_to_go_down) {
                        throw_vel.y = 10.0F;
                    }
                    if (!trying_to_go_up && !trying_to_go_down &&
                        (trying_to_go_left || trying_to_go_right)) {
                        throw_vel.y = -2.0F;
                    }
                    rope_entity->SetCenter(player_center);
                    rope_entity->acc += throw_vel;
                    audio.PlaySoundEffect(SoundEffect::Throw);
                    used_rope = true;
                }
            }
        }

        if (used_rope) {
            Entity& mutable_player = state.entity_manager.entities[entity_idx];
            if (mutable_player.ropes > 0) {
                mutable_player.ropes -= 1;
            }
            mutable_player.rope_throw_delay_countdown = kRopeThrowDelay;
        }
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

        bool attacked = false;
        if (trying_to_attack && attack_delay_countdown == 0) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const entity = state.entity_manager.GetEntityMut(*vid)) {
                    baseball_bat::SetEntityBaseballBat(*entity);
                    entity->pos = player_pos;
                    entity->held_by_vid = player_vid;
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

    if (!loss_of_control) {
        common::TryStompEntitiesBelow(entity_idx, state, graphics, audio, kJumpImpulse);
    }

    //  PICK UPS: MONEY, ETC
    common::CollectTouchingPickups(entity_idx, state, graphics, audio);

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
    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(entity, state);
    if (control.run) {
        entity.vel.x = std::clamp(entity.vel.x, -kMaxRunSpeed, kMaxRunSpeed);
    } else {
        entity.vel.x = std::clamp(entity.vel.x, -kMaxWalkSpeed, kMaxWalkSpeed);
    }
    entity.vel.y = std::clamp(entity.vel.y, -kMaxSpeed, kMaxSpeed);

    if (!entity.IsHorizontallyControlled()) {
        if (entity.grounded) {
            if (state.stage.stage_type != StageType::Ice1 &&
                state.stage.stage_type != StageType::Ice2 &&
                state.stage.stage_type != StageType::Ice3) {
                entity.vel.x *= 0.85F;
            }
        } else {
            entity.vel.x *= 0.85F;
        }
    }
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::player

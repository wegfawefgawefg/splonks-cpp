#include "entities/player.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "state.hpp"

#include <algorithm>
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
    entity.draw_layer = DrawLayer::Middle;
    entity.can_be_stunned = true;
    entity.alignment = Alignment::Ally;
    entity.hurt_on_contact = false;
    entity.sprite_animator.SetSprite(Sprite::PlayerStanding);
}

void StepEntityLogicAsPlayer(std::size_t entity_idx, State& state, Audio& audio, float dt) {
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
                player.holding_vid.reset();
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

    //  PICK UPS: MONEY, ETC
    {
        const Entity& player = state.entity_manager.entities[entity_idx];
        const AABB aabb = player.GetAABB();
        const VID player_vid = player.vid;
        const std::vector<VID> search_results = state.sid.QueryExclude(aabb.tl, aabb.br, player_vid);
        unsigned int money_gained = 0;
        for (const VID& e_vid : search_results) {
            if (const Entity* const e = state.entity_manager.GetEntity(e_vid)) {
                switch (e->type_) {
                case EntityType::Gold:
                    money_gained = 1;
                    audio.PlaySoundEffect(SoundEffect::Gold);
                    break;
                case EntityType::GoldStack:
                    money_gained = 2;
                    audio.PlaySoundEffect(SoundEffect::GoldStack);
                    break;
                default:
                    money_gained = 0;
                    break;
                }
            }
            if (money_gained > 0) {
                state.entity_manager.SetInactiveVid(e_vid);
            }
            Entity& mutable_player = state.entity_manager.entities[entity_idx];
            mutable_player.money += money_gained;
        }
    }

    SyncEntitySpriteToDisplayStatePlayer(state.entity_manager.entities[entity_idx]);
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsPlayer(std::size_t entity_idx, State& state, Audio& audio, float dt) {
    // hanghands_step(entity_idx, state);
    common::JumpingAndClimbingStep(entity_idx, state, audio);

    // custom pre partial euler step for player to apply special velocity clamping.
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.vel += entity.acc;
    if (entity.running) {
        entity.vel.x = std::clamp(entity.vel.x, -kMaxRunSpeed, kMaxRunSpeed);
    } else {
        entity.vel.x = std::clamp(entity.vel.x, -kMaxWalkSpeed, kMaxWalkSpeed);
    }
    entity.vel.y = std::clamp(entity.vel.y, -kMaxSpeed, kMaxSpeed);

    if (!entity.IsHorizontallyControlled()) {
        entity.vel.x *= 0.85F;
        // if entity.grounded {
        //     // if state.stage.stage_type != StageType::Ice {
        //     // }
        // } else {
        //     entity.vel.x *= 0.85;
        // }
    }
    common::DoTileAndEntityCollisions(entity_idx, state, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

void SyncEntitySpriteToDisplayStatePlayer(Entity& entity) {
    const EntityDisplayState display_state = entity.display_state;
    Sprite current_sprite = Sprite::PlayerStanding;
    switch (display_state) {
    case EntityDisplayState::Neutral:
        current_sprite = Sprite::PlayerStanding;
        break;
    case EntityDisplayState::NeutralHolding:
        current_sprite = Sprite::PlayerStandingHolding;
        break;
    case EntityDisplayState::Walk:
        current_sprite = Sprite::PlayerWalking;
        break;
    case EntityDisplayState::WalkHolding:
        current_sprite = Sprite::PlayerWalkingHolding;
        break;
    case EntityDisplayState::Fly:
        current_sprite = Sprite::PlayerFlying;
        break;
    case EntityDisplayState::Dead:
        current_sprite = Sprite::PlayerDead;
        break;
    case EntityDisplayState::Stunned:
        current_sprite = Sprite::PlayerStunned;
        break;
    case EntityDisplayState::Climbing:
        current_sprite = Sprite::PlayerClimbing;
        break;
    case EntityDisplayState::Hanging:
        current_sprite = Sprite::PlayerHanging;
        break;
    case EntityDisplayState::Falling:
        current_sprite = Sprite::PlayerFalling;
        break;
    }
    entity.sprite_animator.SetSprite(current_sprite);
}

} // namespace splonks::entities::player

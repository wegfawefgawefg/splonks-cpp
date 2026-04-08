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

    // PICKING UP THROWING AND DROPPING
    if (!loss_of_control) {
        std::optional<VID> thrown_vid;
        std::optional<std::vector<VID>> trying_to_pick_up_these;
        {
            Entity& player = state.entity_manager.entities[entity_idx];
            if (control.pick_up_drop_pressed) {
                if (player.holding_vid.has_value()) {
                    if (player.holding_timer == 0) {
                        thrown_vid = player.holding_vid;
                        player.holding_vid.reset();
                        player.holding = false;
                        player.holding_timer = kDefaultHoldingTimer;
                    }
                } else {
                    if (!player.IsHanging() && !player.climbing && player.holding_timer == 0) {
                        player.holding_timer = kDefaultHoldingTimer;
                        const AABB aabb = common::GetContactAabbForEntity(player, graphics);
                        trying_to_pick_up_these =
                            state.sid.QueryExclude(aabb.tl, aabb.br, player.vid);
                    }
                }
            }
            if (player.holding_timer > 0) {
                player.holding_timer -= 1;
            }
        }

        std::optional<VID> trying_to_pick_this_up_vid;
        {
            const Entity& player = state.entity_manager.entities[entity_idx];
            const std::optional<VID> player_back_vid = player.back_vid;
            if (trying_to_pick_up_these.has_value()) {
                for (const VID& vid : *trying_to_pick_up_these) {
                    const Entity& candidate = state.entity_manager.entities[vid.id];
                    if (!candidate.can_be_picked_up) {
                        continue;
                    }
                    if (player_back_vid.has_value() && candidate.vid == *player_back_vid) {
                        continue;
                    }
                    trying_to_pick_this_up_vid = vid;
                    break;
                }
            }
        }

        {
            Entity& player = state.entity_manager.entities[entity_idx];
            const VID player_vid = player.vid;
            if (trying_to_pick_this_up_vid.has_value()) {
                player.holding_vid = trying_to_pick_this_up_vid;
                player.holding = true;
                if (Entity* const pick_up_entity =
                        state.entity_manager.GetEntityMut(*trying_to_pick_this_up_vid)) {
                    pick_up_entity->held_by_vid = player_vid;
                }
            }
        }

        {
            const Entity& player = state.entity_manager.entities[entity_idx];
            const VID player_vid = player.vid;
            const Vec2 player_center = player.GetCenter();
            const bool trying_to_go_down = control.down;
            const bool trying_to_go_up = control.up;
            const bool trying_to_go_left = control.left;
            const bool trying_to_go_right = control.right;
            const Vec2 player_size = player.size;

            if (thrown_vid.has_value()) {
                if (Entity* const thrown = state.entity_manager.GetEntityMut(*thrown_vid)) {
                    thrown->has_physics = true;
                    thrown->can_collide = true;
                    thrown->thrown_by = player_vid;
                    thrown->held_by_vid.reset();
                    thrown->thrown_immunity_timer = common::kThrownByImmunityDuration;

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

                    if (player_size.y <= thrown->size.y) {
                        const float delta = std::abs(player_size.y - thrown->size.y) / 2.0F;
                        thrown->SetCenter(player_center - Vec2::New(0.0F, delta));
                    } else {
                        thrown->SetCenter(player_center);
                    }
                    thrown->acc += throw_vel;
                    audio.PlaySoundEffect(SoundEffect::Throw);
                }
            }
        }
    }

    // EQUIPPING TO BACK, TAKING OFF BACK
    if (!loss_of_control) {
        std::optional<VID> take_off_back_vid;
        bool put_held_on_back = false;
        bool equip_action_was_made = false;
        {
            const Entity& player = state.entity_manager.entities[entity_idx];
            if (control.equip_pressed && player.equip_delay_countdown == 0) {
                equip_action_was_made = true;
                if (player.back_vid.has_value()) {
                    take_off_back_vid = player.back_vid;
                }
                if (player.holding_vid.has_value()) {
                    const Entity* const held_thing = state.entity_manager.GetEntity(*player.holding_vid);
                    if (held_thing != nullptr && CanGoOnBack(held_thing->type_)) {
                        put_held_on_back = true;
                        audio.PlaySoundEffect(SoundEffect::Equip);
                    }
                }
            }
        }

        {
            Entity& player = state.entity_manager.entities[entity_idx];
            if (equip_action_was_made) {
                player.equip_delay_countdown = kEquipDelay;
            } else if (player.equip_delay_countdown > 0) {
                player.equip_delay_countdown -= 1;
            }
        }

        const VID player_vid = state.entity_manager.entities[entity_idx].vid;

        if (take_off_back_vid.has_value()) {
            if (Entity* const item_taken_off_back = state.entity_manager.GetEntityMut(*take_off_back_vid)) {
                item_taken_off_back->super_state = EntitySuperState::Idle;
                item_taken_off_back->state = EntityState::Idle;
                item_taken_off_back->has_physics = true;
                item_taken_off_back->can_collide = true;
                item_taken_off_back->display_state = EntityDisplayState::Neutral;
                item_taken_off_back->held_by_vid.reset();
                item_taken_off_back->thrown_by = player_vid;
                item_taken_off_back->thrown_immunity_timer = common::kThrownByImmunityDuration;
            }

            Entity& player = state.entity_manager.entities[entity_idx];
            player.back_vid.reset();
        }

        {
            Entity& player = state.entity_manager.entities[entity_idx];
            if (put_held_on_back) {
                player.back_vid = player.holding_vid;
                player.holding_vid.reset();
                player.holding = false;
            }
        }
    }

    // MOVE HOLDING THING TO HELD POSITION
    {
        const Entity& player = state.entity_manager.entities[entity_idx];
        const std::optional<VID> player_holding_vid = player.holding_vid;
        const LeftOrRight player_facing = player.facing;
        const Vec2 player_center = player.GetCenter() + Vec2::New(0.0F, 1.0F);
        const EntityDisplayState player_display_state = player.display_state;
        const bool player_trying_to_use = control.use_held;

        if (player_holding_vid.has_value()) {
            if (Entity* const holding = state.entity_manager.GetEntityMut(*player_holding_vid)) {
                holding->has_physics = false;
                holding->can_collide = false;
                const Vec2 hold_offset = Vec2::New(4.0F, 0.0F);
                if (player_display_state == EntityDisplayState::Climbing) {
                    holding->draw_layer = DrawLayer::Background;
                } else {
                    holding->draw_layer = DrawLayer::Foreground;
                    holding->display_state = EntityDisplayState::Neutral;
                }
                holding->state =
                    player_trying_to_use ? EntityState::InUse : EntityState::Idle;
                const Vec2 held_pos_target = player_facing == LeftOrRight::Left
                                                 ? player_center +
                                                       Vec2::New(-hold_offset.x, hold_offset.y)
                                                 : player_center + hold_offset;
                holding->SetCenter(held_pos_target);
                holding->grounded = false;
            }
        }
    }

    // MOVE THING ON BACK TO BACK POSITION
    {
        const Entity& player = state.entity_manager.entities[entity_idx];
        const VID player_vid = player.vid;
        const std::optional<VID> player_back_vid = player.back_vid;
        const LeftOrRight player_facing = player.facing;
        const Vec2 player_center = player.GetCenter();
        const EntityDisplayState player_display_state = player.display_state;
        const bool player_trying_to_use = control.use_held;

        if (player_back_vid.has_value()) {
            if (Entity* const back_item = state.entity_manager.GetEntityMut(*player_back_vid)) {
                back_item->has_physics = false;
                back_item->can_collide = false;
                back_item->facing = player_facing;
                back_item->held_by_vid = player_vid;
                back_item->super_state = EntitySuperState::EquippedToBack;
                back_item->state = player_trying_to_use ? EntityState::InUse : EntityState::Idle;

                Vec2 back_offset = Vec2::New(-3.0F, 0.0F);
                if (player_display_state == EntityDisplayState::Climbing) {
                    back_offset = Vec2::New(-2.0F, 0.0F);
                    back_item->display_state = EntityDisplayState::Climbing;
                    back_item->draw_layer = DrawLayer::Foreground;
                } else if (player_display_state == EntityDisplayState::Hanging) {
                    back_offset = Vec2::New(-7.0F, 4.0F);
                    back_item->display_state = EntityDisplayState::Hanging;
                    back_item->draw_layer = DrawLayer::Foreground;
                } else {
                    back_item->draw_layer = DrawLayer::Background;
                    back_item->display_state = EntityDisplayState::Neutral;
                }

                const Vec2 held_pos_target = player_facing == LeftOrRight::Left
                                                 ? player_center +
                                                       Vec2::New(-back_offset.x, back_offset.y)
                                                 : player_center + back_offset;
                back_item->SetCenter(held_pos_target);
                back_item->grounded = false;
            }
        }
    }

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
        Entity& player = state.entity_manager.entities[entity_idx];
        const bool player_grounded = player.grounded;
        const AABB player_aabb = common::GetContactAabbForEntity(player, graphics);
        const VID player_vid = player.vid;
        const Vec2 player_vel = player.vel;

        bool ready_to_push = false;
        if (player_grounded) {
            const AABB try_to_push_zone = {
                .tl = player_aabb.tl - Vec2::New(6.0F, 0.0F),
                .br = player_aabb.br + Vec2::New(6.0F, 0.0F),
            };
            const std::vector<VID> search_results =
                state.sid.QueryExclude(try_to_push_zone.tl, try_to_push_zone.br, player_vid);
            for (const VID& vid : search_results) {
                const Entity& candidate = state.entity_manager.entities[vid.id];
                if (candidate.type_ != EntityType::Block) {
                    continue;
                }
                if (Entity* const block_entity = state.entity_manager.GetEntityMut(vid)) {
                    ready_to_push = true;
                    const float push_zone_left_x = player_aabb.tl.x - 1.0F;
                    const float push_zone_right_x = player_aabb.br.x + 1.0F;
                    const auto [block_tl, block_br] = block_entity->GetBounds();
                    float block_x_acc_delta = 0.0F;
                    if (player_vel.x > 0.0F && block_br.x > push_zone_left_x &&
                        block_tl.x > push_zone_left_x) {
                        block_x_acc_delta = block::kBlockPushAcc;
                    } else if (player_vel.x < 0.0F && block_tl.x < push_zone_right_x &&
                               block_br.x < push_zone_right_x) {
                        block_x_acc_delta = -block::kBlockPushAcc;
                    }
                    block_entity->acc.x += block_x_acc_delta;
                    break;
                }
            }
        }

        if (ready_to_push) {
            player.state = EntityState::Pushing;
        } else if (player.state == EntityState::Pushing) {
            player.state = EntityState::Idle;
        }
    }

    //  PICK UPS: MONEY, ETC
    if (!loss_of_control) {
        const Entity& player = state.entity_manager.entities[entity_idx];
        const VID player_vid = player.vid;
        const AABB aabb = common::GetContactAabbForEntity(player, graphics);
        const AABB player_jump_foot = {
            .tl = Vec2::New(aabb.tl.x, aabb.br.y - 2.0F),
            .br = aabb.br,
        };

        bool jumped = false;
        const std::vector<VID> results =
            state.sid.QueryExclude(player_jump_foot.tl, player_jump_foot.br, player_vid);
        if (!results.empty()) {
            if (Entity* const entity = state.entity_manager.GetEntityMut(results.front())) {
                if (!entity->impassable && entity->can_collide &&
                    entity->super_state != EntitySuperState::Dead &&
                    entity->super_state != EntitySuperState::Stunned &&
                    entity->alignment == Alignment::Enemy) {
                    jumped = true;

                    audio.PlaySoundEffect(SoundEffect::Jump);

                    const common::DamageResult damage_result = common::TryToDamageEntity(
                        entity->vid.id,
                        state,
                        audio,
                        DamageType::JumpOn,
                        1);
                    if (Entity* const jumped_entity =
                            state.entity_manager.GetEntityMut(results.front())) {
                        if (jumped_entity->can_be_stunned) {
                            jumped_entity->super_state = EntitySuperState::Stunned;
                            jumped_entity->stun_timer = common::kDefaultStunTimer;
                        }
                        if (damage_result == common::DamageResult::Died ||
                            damage_result == common::DamageResult::Hurt) {
                            jumped_entity->thrown_by = player_vid;
                            jumped_entity->thrown_immunity_timer =
                                common::kThrownByImmunityDuration;
                        }
                    }
                }
            }
        }
        if (jumped) {
            Entity& mutable_player = state.entity_manager.entities[entity_idx];
            mutable_player.vel.y = -kJumpImpulse;
        }
    }

    //  PICK UPS: MONEY, ETC
    {
        const Entity& player = state.entity_manager.entities[entity_idx];
        const AABB aabb = common::GetContactAabbForEntity(player, graphics);
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

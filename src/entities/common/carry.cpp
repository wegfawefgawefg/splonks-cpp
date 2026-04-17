#include "entities/common/common.hpp"

#include "entities/player.hpp"
#include "entity/archetype_restore.hpp"
#include "controls.hpp"
#include "world_query.hpp"

#include <optional>
#include <vector>

namespace splonks::entities::common {

namespace {

void ApplyHeldState(Entity& entity) {
    entity.thrown_by.reset();
    entity.thrown_immunity_timer = 0;
    entity.projectile_contact_damage_type = DamageType::Attack;
    entity.projectile_contact_damage_amount = 1;
    entity.projectile_contact_timer = 0;
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);

    if (entity.type_ != EntityType::Damsel) {
        return;
    }

    entity.condition = EntityCondition::Stunned;
    entity.stun_timer = kDefaultStunTimer;
    TrySetAnimation(entity, EntityDisplayState::Stunned);
}

void SyncHeldAttachmentForHolder(
    const Entity& holder,
    State& state,
    const Graphics& graphics
) {
    if (!holder.holding_vid.has_value()) {
        return;
    }

    Entity* const holding = state.entity_manager.GetEntityMut(*holder.holding_vid);
    if (holding == nullptr || !holding->active) {
        return;
    }

    holding->has_physics = false;
    holding->can_collide = false;
    holding->held_by_vid = holder.vid;
    holding->attachment_mode = AttachmentMode::Held;
    ApplyHeldState(*holding);
    holding->facing = holder.facing;
    holding->draw_layer = HasMovementFlag(holder, EntityMovementFlag::Climbing)
                              ? DrawLayer::Background
                              : DrawLayer::Foreground;

    const Vec2 hold_offset = Vec2::New(4.0F, 0.0F);
    const Vec2 holder_center = holder.GetCenter() + Vec2::New(0.0F, 1.0F);
    const Vec2 held_pos_target =
        holder.facing == LeftOrRight::Left
            ? holder_center + Vec2::New(-hold_offset.x, hold_offset.y)
            : holder_center + hold_offset;
    SetVisualCenterForEntity(*holding, graphics, held_pos_target);
    holding->grounded = false;
    state.UpdateSidForEntity(holding->vid.id, graphics);
}

void SyncBackAttachmentForHolder(
    const Entity& holder,
    State& state,
    const Graphics& graphics
) {
    if (!holder.back_vid.has_value()) {
        return;
    }

    Entity* const back_item = state.entity_manager.GetEntityMut(*holder.back_vid);
    if (back_item == nullptr || !back_item->active) {
        return;
    }

    back_item->has_physics = false;
    back_item->can_collide = false;
    back_item->facing = holder.facing;
    back_item->held_by_vid = holder.vid;
    back_item->attachment_mode = AttachmentMode::Back;

    const bool holder_climbing = HasMovementFlag(holder, EntityMovementFlag::Climbing);
    const bool holder_hanging = HasMovementFlag(holder, EntityMovementFlag::Hanging);

    Vec2 back_offset = Vec2::New(-3.0F, 0.0F);
    if (holder_climbing) {
        back_offset = Vec2::New(-2.0F, 0.0F);
        TrySetAnimation(*back_item, EntityDisplayState::Climbing);
        back_item->draw_layer = DrawLayer::Foreground;
    } else if (holder_hanging) {
        back_offset = Vec2::New(-7.0F, 4.0F);
        TrySetAnimation(*back_item, EntityDisplayState::Hanging);
        back_item->draw_layer = DrawLayer::Foreground;
    } else {
        back_item->draw_layer = DrawLayer::Background;
        TrySetAnimation(*back_item, EntityDisplayState::Neutral);
    }

    const Vec2 holder_center = holder.GetCenter();
    const Vec2 held_pos_target =
        holder.facing == LeftOrRight::Left
            ? holder_center + Vec2::New(-back_offset.x, back_offset.y)
            : holder_center + back_offset;
    back_item->SetCenter(held_pos_target);
    back_item->grounded = false;
    state.UpdateSidForEntity(back_item->vid.id, graphics);
}

} // namespace


void ReleaseEntityFromHolder(Entity& entity, State& state) {
    if (!entity.held_by_vid.has_value()) {
        return;
    }

    if (Entity* const holder = state.entity_manager.GetEntityMut(*entity.held_by_vid)) {
        if (holder->holding_vid.has_value() && *holder->holding_vid == entity.vid) {
            holder->holding_vid.reset();
            holder->holding = false;
            holder->holding_timer = kDefaultHoldingTimer;
        }
        if (holder->back_vid.has_value() && *holder->back_vid == entity.vid) {
            holder->back_vid.reset();
        }
    }

    entity.held_by_vid.reset();
    entity.attachment_mode = AttachmentMode::None;
    StopUsingEntity(entity);
    RestoreEntityHasPhysicsFromArchetype(entity);
    RestoreEntityCanCollideFromArchetype(entity);
    RestoreEntityDrawLayerFromArchetype(entity);
    entity.grounded = false;
}

void DropHeldItemFromEntity(Entity& entity, State& state) {
    if (!entity.holding_vid.has_value()) {
        return;
    }

    Entity* const held = state.entity_manager.GetEntityMut(*entity.holding_vid);
    entity.holding_vid.reset();
    entity.holding = false;
    entity.holding_timer = kDefaultHoldingTimer;
    if (held == nullptr) {
        return;
    }

    const float throw_x = entity.facing == LeftOrRight::Left ? -3.0F : 3.0F;
    held->held_by_vid.reset();
    held->attachment_mode = AttachmentMode::None;
    StopUsingEntity(*held);
    RestoreEntityHasPhysicsFromArchetype(*held);
    RestoreEntityCanCollideFromArchetype(*held);
    RestoreEntityDrawLayerFromArchetype(*held);
    held->grounded = false;
    held->thrown_by = entity.vid;
    held->thrown_immunity_timer = kThrownByImmunityDuration;
    held->projectile_contact_damage_type = DamageType::Attack;
    held->projectile_contact_damage_amount = 1;
    held->projectile_contact_timer = kProjectileContactDuration;
    held->vel = Vec2::New(throw_x, -1.0F);
    held->acc = Vec2::New(0.0F, 0.0F);
}

void CleanupInactiveCarryReferences(std::size_t entity_idx, State& state) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.holding_vid.has_value()) {
        const Entity* const holding = state.entity_manager.GetEntity(*entity.holding_vid);
        if (holding == nullptr || !holding->active) {
            entity.holding_vid.reset();
            entity.holding = false;
            entity.holding_timer = kDefaultHoldingTimer;
        }
    }

    if (entity.back_vid.has_value()) {
        const Entity* const back_item = state.entity_manager.GetEntity(*entity.back_vid);
        if (back_item == nullptr || !back_item->active) {
            entity.back_vid.reset();
        }
    }
}

void UpdateCarryAndBackItems(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    CleanupInactiveCarryReferences(entity_idx, state);

    const bool loss_of_control =
        state.entity_manager.entities[entity_idx].condition == EntityCondition::Stunned;
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(
            state.entity_manager.entities[entity_idx],
            state
        );

    if (!loss_of_control) {
        std::optional<VID> thrown_vid;
        std::optional<std::vector<VID>> trying_to_pick_up_these;
        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            if (control.pick_up_drop_pressed) {
                if (entity.holding_vid.has_value()) {
                    if (entity.holding_timer == 0) {
                        thrown_vid = entity.holding_vid;
                        entity.holding_vid.reset();
                        entity.holding = false;
                        entity.holding_timer = kDefaultHoldingTimer;
                    }
                } else {
                    if (!entity.IsHanging() && !entity.IsClimbing() && entity.holding_timer == 0) {
                        entity.holding_timer = kDefaultHoldingTimer;
                        const AABB aabb = GetContactAabbForEntity(entity, graphics);
                        trying_to_pick_up_these =
                            QueryEntitiesInAabb(state, aabb, entity.vid);
                    }
                }
            }
            if (entity.holding_timer > 0) {
                entity.holding_timer -= 1;
            }
        }

        std::optional<VID> trying_to_pick_this_up_vid;
        {
            const Entity& entity = state.entity_manager.entities[entity_idx];
            const std::optional<VID> entity_back_vid = entity.back_vid;
            if (trying_to_pick_up_these.has_value()) {
                for (const VID& vid : *trying_to_pick_up_these) {
                    const Entity& candidate = state.entity_manager.entities[vid.id];
                    const bool dead_or_stunned =
                        candidate.condition == EntityCondition::Dead ||
                        candidate.condition == EntityCondition::Stunned;
                    const bool can_pick_up_candidate =
                        candidate.can_be_picked_up &&
                        (!candidate.can_only_be_picked_up_if_dead_or_stunned ||
                         dead_or_stunned);
                    if (!can_pick_up_candidate) {
                        continue;
                    }
                    if (entity_back_vid.has_value() && candidate.vid == *entity_back_vid) {
                        continue;
                    }
                    trying_to_pick_this_up_vid = vid;
                    break;
                }
            }
        }

        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            const VID entity_vid = entity.vid;
            if (trying_to_pick_this_up_vid.has_value()) {
                entity.holding_vid = trying_to_pick_this_up_vid;
                entity.holding = true;
                if (Entity* const pick_up_entity =
                        state.entity_manager.GetEntityMut(*trying_to_pick_this_up_vid)) {
                    pick_up_entity->held_by_vid = entity_vid;
                    pick_up_entity->attachment_mode = AttachmentMode::Held;
                    ApplyHeldState(*pick_up_entity);
                }
            }
        }

        {
            const Entity& entity = state.entity_manager.entities[entity_idx];
            const VID entity_vid = entity.vid;
            const Vec2 entity_center = entity.GetCenter();
            const bool trying_to_go_down = control.down;
            const bool trying_to_go_up = control.up;
            const bool trying_to_go_left = control.left;
            const bool trying_to_go_right = control.right;
            const Vec2 entity_size = entity.size;

            if (thrown_vid.has_value()) {
                if (Entity* const thrown = state.entity_manager.GetEntityMut(*thrown_vid)) {
                    thrown->has_physics = true;
                    thrown->can_collide = true;
                    thrown->thrown_by = entity_vid;
                    thrown->held_by_vid.reset();
                    thrown->attachment_mode = AttachmentMode::None;
                    StopUsingEntity(*thrown);
                    thrown->thrown_immunity_timer = kThrownByImmunityDuration;
                    thrown->projectile_contact_damage_type = DamageType::Attack;
                    thrown->projectile_contact_damage_amount = 1;
                    thrown->projectile_contact_timer = kProjectileContactDuration;

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

                    if (entity_size.y <= thrown->size.y) {
                        const float delta = std::abs(entity_size.y - thrown->size.y) / 2.0F;
                        thrown->SetCenter(entity_center - Vec2::New(0.0F, delta));
                    } else {
                        thrown->SetCenter(entity_center);
                    }
                    thrown->acc += throw_vel;
                    state.UpdateSidForEntity(thrown->vid.id, graphics);
                    audio.PlaySoundEffect(SoundEffect::Throw);
                }
            }
        }
    }

    if (!loss_of_control) {
        std::optional<VID> take_off_back_vid;
        bool put_held_on_back = false;
        bool equip_action_was_made = false;
        {
            const Entity& entity = state.entity_manager.entities[entity_idx];
            if (control.equip_pressed && entity.equip_delay_countdown == 0) {
                equip_action_was_made = true;
                if (entity.back_vid.has_value()) {
                    take_off_back_vid = entity.back_vid;
                }
                if (entity.holding_vid.has_value()) {
                    const Entity* const held_thing = state.entity_manager.GetEntity(*entity.holding_vid);
                    if (held_thing != nullptr && held_thing->can_go_on_back) {
                        put_held_on_back = true;
                        audio.PlaySoundEffect(SoundEffect::Equip);
                    }
                }
            }
        }

        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            if (equip_action_was_made) {
                entity.equip_delay_countdown = player::kEquipDelay;
            } else if (entity.equip_delay_countdown > 0) {
                entity.equip_delay_countdown -= 1;
            }
        }

        const VID entity_vid = state.entity_manager.entities[entity_idx].vid;

        if (take_off_back_vid.has_value()) {
            if (Entity* const item_taken_off_back = state.entity_manager.GetEntityMut(*take_off_back_vid)) {
                item_taken_off_back->has_physics = true;
                item_taken_off_back->can_collide = true;
                TrySetAnimation(*item_taken_off_back, EntityDisplayState::Neutral);
                item_taken_off_back->held_by_vid.reset();
                item_taken_off_back->attachment_mode = AttachmentMode::None;
                StopUsingEntity(*item_taken_off_back);
                item_taken_off_back->thrown_by = entity_vid;
                item_taken_off_back->thrown_immunity_timer = kThrownByImmunityDuration;
                item_taken_off_back->projectile_contact_damage_type = DamageType::Attack;
                item_taken_off_back->projectile_contact_damage_amount = 1;
                item_taken_off_back->projectile_contact_timer = kProjectileContactDuration;
            }

            Entity& entity = state.entity_manager.entities[entity_idx];
            entity.back_vid.reset();
        }

        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            if (put_held_on_back) {
                entity.back_vid = entity.holding_vid;
                entity.holding_vid.reset();
                entity.holding = false;
            }
        }
    }

    {
        const Entity& entity = state.entity_manager.entities[entity_idx];
        if (entity.holding_vid.has_value()) {
            if (Entity* const holding = state.entity_manager.GetEntityMut(*entity.holding_vid)) {
                if (control.use_held) {
                    UseEntity(*holding, entity.vid, AttachmentMode::Held);
                } else {
                    StopUsingEntity(*holding);
                }
            }
        }
        if (entity.back_vid.has_value()) {
            if (Entity* const back_item = state.entity_manager.GetEntityMut(*entity.back_vid)) {
                if (control.use_back) {
                    UseEntity(*back_item, entity.vid, AttachmentMode::Back);
                } else {
                    StopUsingEntity(*back_item);
                }
            }
        }
    }
}

void SyncEntityAttachments(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    CleanupInactiveCarryReferences(entity_idx, state);
    const Entity& holder = state.entity_manager.entities[entity_idx];
    if (!holder.active) {
        return;
    }

    SyncHeldAttachmentForHolder(holder, state, graphics);
    SyncBackAttachmentForHolder(holder, state, graphics);
}

} // namespace splonks::entities::common

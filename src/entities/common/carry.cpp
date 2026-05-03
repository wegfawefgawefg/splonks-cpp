#include "entities/common/common.hpp"

#include "entities/player.hpp"
#include "entity/archetype.hpp"
#include "entity/archetype_restore.hpp"
#include "controls.hpp"
#include "network/net_event.hpp"
#include "network/net_session.hpp"
#include "world_query.hpp"

#include <cmath>
#include <optional>
#include <vector>

namespace splonks::entities::common {

namespace {

void ApplyHeldState(Entity& entity) {
    entity.thrown_by.reset();
    entity.thrown_immunity_timer = 0;
    const EntityArchetype& archetype = GetEntityArchetype(entity.type_);
    entity.projectile_contact_damage_type = archetype.projectile_contact_damage_type;
    entity.projectile_contact_damage_amount = archetype.projectile_contact_damage_amount;
    entity.projectile_contact_timer = 0;
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);
    RemoveEffect(entity, EffectId::NoGravityUntilContact);
    entity.rotation = 0.0F;

}

void SnapPlacedAttachmentToPixels(Entity& entity) {
    entity.pos = Vec2::New(std::round(entity.pos.x), std::round(entity.pos.y));
}

bool IsAttachmentDriven(const Entity& entity) {
    return entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None;
}

bool HasAttachmentReference(const Entity& entity, const State& state) {
    if (IsAttachmentDriven(entity)) {
        return true;
    }
    for (const Entity& candidate_holder : state.entity_manager.entities) {
        if (!candidate_holder.active || candidate_holder.vid == entity.vid) {
            continue;
        }
        if ((candidate_holder.holding_vid.has_value() &&
             *candidate_holder.holding_vid == entity.vid) ||
            (candidate_holder.back_vid.has_value() &&
             *candidate_holder.back_vid == entity.vid)) {
            return true;
        }
    }
    return false;
}

bool IsPlayerEntity(const Entity& entity, const State& state) {
    return state.players.FindByEntityVid(entity.vid) != nullptr;
}

bool IsVidInHolderChain(VID needle, const Entity& entity, const State& state) {
    std::optional<VID> holder_vid = entity.held_by_vid;
    constexpr int kMaxCarryChainDepth = 16;
    for (int depth = 0; depth < kMaxCarryChainDepth && holder_vid.has_value(); ++depth) {
        if (*holder_vid == needle) {
            return true;
        }
        const Entity* const holder = state.entity_manager.GetEntity(*holder_vid);
        if (holder == nullptr || !holder->active) {
            return false;
        }
        holder_vid = holder->held_by_vid;
    }
    return false;
}

bool CanPickUpCandidate(const Entity& picker, const Entity& candidate, const State& state) {
    if (!candidate.can_be_picked_up || candidate.vid == picker.vid) {
        return false;
    }
    if (picker.back_vid.has_value() && candidate.vid == *picker.back_vid) {
        return false;
    }
    if (IsAttachmentDriven(candidate)) {
        return false;
    }
    if (IsVidInHolderChain(candidate.vid, picker, state)) {
        return false;
    }

    const bool dead_or_stunned =
        candidate.condition == EntityCondition::Dead ||
        candidate.condition == EntityCondition::Stunned;
    return !candidate.can_only_be_picked_up_if_dead_or_stunned || dead_or_stunned;
}

network::NetEntityId GetReplicatedEntityId(State& state, const Entity& entity) {
    if (const std::optional<network::NetEntityId> linked =
            state.net_session.FindNetEntityId(entity.vid)) {
        return *linked;
    }

    if (const std::optional<PlayerId> player_id = state.players.FindPlayerIdForEntity(entity.vid)) {
        const network::NetEntityId player_entity_id = network::MakePlayerNetEntityId(*player_id);
        state.net_session.LinkEntity(player_entity_id, entity.vid);
        return player_entity_id;
    }

    const network::NetEntityId deterministic_stage_id =
        static_cast<network::NetEntityId>(entity.vid.id) + 1U;
    state.net_session.LinkEntity(deterministic_stage_id, entity.vid);
    return deterministic_stage_id;
}

void EmitHeldNetworkEvent(State& state, const Entity& holder, const Entity& held) {
    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }

    const network::NetEntityId held_id = GetReplicatedEntityId(state, held);
    state.net_session.SetEntityOwner(held_id, state.net_session.local_player_id);

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityHeld;
    event.payload = network::EntityHeldEvent{
        .holder_id = GetReplicatedEntityId(state, holder),
        .held_id = held_id,
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EmitThrownNetworkEvent(State& state, const Entity& thrower, const Entity& thrown, Vec2 throw_velocity) {
    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }

    const network::NetEntityId thrown_id = GetReplicatedEntityId(state, thrown);
    state.net_session.SetEntityOwner(thrown_id, state.net_session.local_player_id);

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityThrown;
    event.payload = network::EntityThrownEvent{
        .entity_id = thrown_id,
        .pos = thrown.pos,
        .vel = throw_velocity,
        .thrower_id = GetReplicatedEntityId(state, thrower),
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EmitDroppedNetworkEvent(State& state, const Entity& entity) {
    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityDropped;
    event.payload = network::EntityDroppedEvent{
        .entity_id = GetReplicatedEntityId(state, entity),
        .pos = entity.pos,
        .vel = entity.vel,
    };
    state.net_session.EnqueueLocalEvent(event);
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

    const EntityArchetype& holding_archetype = GetEntityArchetype(holding->type_);
    const bool preserve_held_aim = holding_archetype.preserve_held_aim;
    const LeftOrRight aimed_facing = holding->facing;
    const float aimed_rotation = holding->rotation;

    holding->has_physics = false;
    holding->can_collide = false;
    holding->held_by_vid = holder.vid;
    holding->attachment_mode = AttachmentMode::Held;
    ApplyHeldState(*holding);
    holding->facing = preserve_held_aim ? aimed_facing : holder.facing;
    if (preserve_held_aim) {
        holding->rotation = aimed_rotation;
    }
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
    SnapPlacedAttachmentToPixels(*holding);
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
    SnapPlacedAttachmentToPixels(*back_item);
    back_item->grounded = false;
    state.UpdateSidForEntity(back_item->vid.id, graphics);
}

} // namespace

void AttachEntityAsHeld(Entity& holder, Entity& held) {
    holder.holding_vid = held.vid;
    holder.holding = true;
    held.held_by_vid = holder.vid;
    held.attachment_mode = AttachmentMode::Held;
    held.has_physics = false;
    held.can_collide = false;
    ApplyHeldState(held);
}


void ReleaseEntityFromHolder(Entity& entity, State& state) {
    if (entity.held_by_vid.has_value()) {
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
    }

    for (Entity& candidate_holder : state.entity_manager.entities) {
        if (!candidate_holder.active || candidate_holder.vid == entity.vid) {
            continue;
        }
        if (candidate_holder.holding_vid.has_value() &&
            *candidate_holder.holding_vid == entity.vid) {
            candidate_holder.holding_vid.reset();
            candidate_holder.holding = false;
            candidate_holder.holding_timer = kDefaultHoldingTimer;
        }
        if (candidate_holder.back_vid.has_value() &&
            *candidate_holder.back_vid == entity.vid) {
            candidate_holder.back_vid.reset();
        }
    }

    entity.held_by_vid.reset();
    entity.attachment_mode = AttachmentMode::None;
    StopUsingEntity(entity);
    RestoreEntityHasPhysicsFromArchetype(entity);
    RestoreEntityCanCollideFromArchetype(entity);
    RestoreEntityDrawLayerFromArchetype(entity);
    entity.grounded = false;
    RemoveEffect(entity, EffectId::NoGravityUntilContact);
}

void ReleaseEntityFromHolderAndEmitNetwork(Entity& entity, State& state) {
    if (!HasAttachmentReference(entity, state)) {
        return;
    }
    ReleaseEntityFromHolder(entity, state);
    EmitDroppedNetworkEvent(state, entity);
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

    if (IsPlayerEntity(*held, state)) {
        ReleaseEntityFromHolder(*held, state);
        held->vel = Vec2::New(0.0F, 0.0F);
        held->acc = Vec2::New(0.0F, 0.0F);
        EmitDroppedNetworkEvent(state, *held);
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
    const EntityArchetype& held_archetype = GetEntityArchetype(held->type_);
    held->can_apply_projectile_contact = held_archetype.can_apply_projectile_contact;
    held->projectile_contact_damage_type = held_archetype.projectile_contact_damage_type;
    held->projectile_contact_damage_amount = held_archetype.projectile_contact_damage_amount;
    held->projectile_contact_timer = kProjectileContactDuration;
    held->vel = Vec2::New(throw_x, -1.0F);
    held->acc = Vec2::New(0.0F, 0.0F);
    RemoveEffect(*held, EffectId::NoGravityUntilContact);
    EmitThrownNetworkEvent(state, entity, *held, held->vel);
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
    (void)audio;
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
            Entity& entity = state.entity_manager.entities[entity_idx];
            if (trying_to_pick_up_these.has_value()) {
                for (const VID& vid : *trying_to_pick_up_these) {
                    const Entity& candidate = state.entity_manager.entities[vid.id];
                    if (!CanPickUpCandidate(entity, candidate, state)) {
                        continue;
                    }
                    trying_to_pick_this_up_vid = vid;
                    break;
                }
            }
        }

        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            if (trying_to_pick_this_up_vid.has_value()) {
                if (Entity* const pick_up_entity =
                        state.entity_manager.GetEntityMut(*trying_to_pick_this_up_vid)) {
                    AttachEntityAsHeld(entity, *pick_up_entity);
                    EmitHeldNetworkEvent(state, entity, *pick_up_entity);
                }
            }
        }

        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            const VID entity_vid = entity.vid;
            const Vec2 entity_center = entity.GetCenter();
            const bool trying_to_go_down = control.down;
            const bool trying_to_go_up = control.up;
            const bool trying_to_go_left = control.left;
            const bool trying_to_go_right = control.right;
            const Vec2 entity_size = entity.size;
            const float mitt_throw_boost =
                entity.grounded && trying_to_go_down
                    ? 0.0F
                    : GetModifiedEffectValue(entity, EffectModifierTarget::ThrowHorizontalBoost, 0.0F);
            const bool mitt_throw = mitt_throw_boost != 0.0F;

            if (thrown_vid.has_value()) {
                if (Entity* const thrown = state.entity_manager.GetEntityMut(*thrown_vid)) {
                    thrown->has_physics = true;
                    thrown->can_collide = true;
                    thrown->thrown_by = entity_vid;
                    thrown->held_by_vid.reset();
                    thrown->attachment_mode = AttachmentMode::None;
                    StopUsingEntity(*thrown);
                    thrown->thrown_immunity_timer = kThrownByImmunityDuration;
                    const EntityArchetype& thrown_archetype = GetEntityArchetype(thrown->type_);
                    thrown->can_apply_projectile_contact = thrown_archetype.can_apply_projectile_contact;
                    thrown->projectile_contact_damage_type = thrown_archetype.projectile_contact_damage_type;
                    thrown->projectile_contact_damage_amount = thrown_archetype.projectile_contact_damage_amount;
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
                    if (mitt_throw) {
                        const float throw_direction =
                            throw_vel.x < 0.0F ? -1.0F :
                            throw_vel.x > 0.0F ? 1.0F :
                            entity.facing == LeftOrRight::Left ? -1.0F : 1.0F;
                        throw_vel.x += throw_direction * mitt_throw_boost;
                        if (!trying_to_go_up && !trying_to_go_down) {
                            throw_vel.y = -0.4F;
                        } else if (trying_to_go_down) {
                            throw_vel.y = 6.0F;
                        }
                        DispatchEffectEventToEntity(
                            entity,
                            state,
                            &audio,
                            EffectEvent{
                                .type = EffectEventType::Throw,
                                .actor_vid = entity.vid,
                                .target_vid = thrown->vid,
                                .world_pos = thrown->GetCenter(),
                            }
                        );
                    } else {
                        RemoveEffect(*thrown, EffectId::NoGravityUntilContact);
                    }

                    if (entity_size.y <= thrown->size.y) {
                        const float delta = std::abs(entity_size.y - thrown->size.y) / 2.0F;
                        thrown->SetCenter(entity_center - Vec2::New(0.0F, delta));
                    } else {
                        thrown->SetCenter(entity_center);
                    }
                    thrown->acc += throw_vel * entity.throw_velocity_scale;
                    state.UpdateSidForEntity(thrown->vid.id, graphics);
                    EmitThrownNetworkEvent(state, entity, *thrown, throw_vel * entity.throw_velocity_scale);
                    (void)PlayEntityCenterSoundEmitter(state, entity, audio_asset_ids::Throw);
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
            if (entity.equip_delay_countdown == 0) {
                if (control.buy_pressed && entity.back_vid.has_value()) {
                    equip_action_was_made = true;
                    take_off_back_vid = entity.back_vid;
                }
                if (control.equip_pressed && entity.holding_vid.has_value()) {
                    const Entity* const held_thing = state.entity_manager.GetEntity(*entity.holding_vid);
                    if (held_thing != nullptr && held_thing->can_go_on_back) {
                        equip_action_was_made = true;
                        put_held_on_back = true;
                        (void)PlayEntityCenterSoundEmitter(state, entity, audio_asset_ids::Equip);
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
                const EntityArchetype& back_item_archetype = GetEntityArchetype(item_taken_off_back->type_);
                item_taken_off_back->can_apply_projectile_contact =
                    back_item_archetype.can_apply_projectile_contact;
                item_taken_off_back->projectile_contact_damage_type = back_item_archetype.projectile_contact_damage_type;
                item_taken_off_back->projectile_contact_damage_amount = back_item_archetype.projectile_contact_damage_amount;
                item_taken_off_back->projectile_contact_timer = kProjectileContactDuration;
                RemoveEffect(*item_taken_off_back, EffectId::NoGravityUntilContact);
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

#include "entities/common/common.hpp"

#include "entities/player.hpp"
#include "entity/archetype.hpp"
#include "entity/archetype_restore.hpp"
#include "controls.hpp"
#include "gameplay_messages.hpp"
#include "gameplay_authority.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kForcedPlayerDropHarmCooldownFrames = 12;

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
    entity.hang_side.reset();
    entity.hang_count = 0;
    entity.climb_detach_cooldown = 0;
    entity.coyote_time = 0;
    entity.jump_hold_gravity_frames_remaining = 0;
    entity.jumped_this_frame = false;
    entity.grounded = false;
    entity.movement_flags = 0;
    if (entity.condition == EntityCondition::Normal) {
        TrySetAnimation(entity, EntityDisplayState::Neutral);
    }
}

void TrackChangedEntity(std::vector<VID>& changed_entities, VID vid) {
    if (std::find(changed_entities.begin(), changed_entities.end(), vid) != changed_entities.end()) {
        return;
    }
    changed_entities.push_back(vid);
}

void RestoreDetachedCarryEntity(Entity& entity) {
    entity.held_by_vid.reset();
    entity.attachment_mode = AttachmentMode::None;
    StopUsingEntity(entity);
    RestoreEntityHasPhysicsFromArchetype(entity);
    RestoreEntityCanCollideFromArchetype(entity);
    RestoreEntityDrawLayerFromArchetype(entity);
    entity.grounded = false;
    RemoveEffect(entity, EffectId::NoGravityUntilContact);
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

bool ShouldRequestCarryAction(const State& state, const Entity& entity) {
    return state.net_session.role == network::NetRole::Peer &&
           HasLocalGameplayAuthorityForInteractionSource(state, entity.vid);
}

void EmitCarryActionRequest(
    State& state,
    GameplayActionKind kind,
    VID source_vid,
    std::optional<VID> target_vid,
    Vec2 velocity = Vec2::New(0.0F, 0.0F)
) {
    world_ops::RequestGameplayAction(
        state,
        GameplayActionRequested{
            .kind = kind,
            .source_vid = source_vid,
            .target_vid = target_vid,
            .velocity = velocity,
        }
    );
}

void EmitAttachmentUseActionRequest(
    State& state,
    GameplayActionKind kind,
    VID holder_vid,
    VID item_vid,
    bool pressed,
    const controls::ControlIntent& control
) {
    const int direction_x =
        control.left && !control.right ? -1 :
        control.right && !control.left ? 1 :
        0;
    const int direction_y =
        control.up && !control.down ? -1 :
        control.down && !control.up ? 1 :
        0;
    world_ops::RequestGameplayAction(
        state,
        GameplayActionRequested{
            .kind = kind,
            .source_vid = holder_vid,
            .target_vid = item_vid,
            .direction = IVec2::New(direction_x, direction_y),
            .param_a = pressed ? 1U : 0U,
        }
    );
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
    holding->vel = Vec2::New(0.0F, 0.0F);
    holding->acc = Vec2::New(0.0F, 0.0F);
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
    back_item->vel = Vec2::New(0.0F, 0.0F);
    back_item->acc = Vec2::New(0.0F, 0.0F);
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

void ApplyThrowState(
    Entity& thrower,
    Entity& thrown,
    Vec2 throw_velocity,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    ReleaseEntityFromHolder(thrown, state);

    thrown.thrown_by = thrower.vid;
    thrown.thrown_immunity_timer = kThrownByImmunityDuration;
    const EntityArchetype& thrown_archetype = GetEntityArchetype(thrown.type_);
    thrown.can_apply_projectile_contact = thrown_archetype.can_apply_projectile_contact;
    thrown.projectile_contact_damage_type = thrown_archetype.projectile_contact_damage_type;
    thrown.projectile_contact_damage_amount = thrown_archetype.projectile_contact_damage_amount;
    thrown.projectile_contact_timer = kProjectileContactDuration;

    if (GetModifiedEffectValue(thrower, EffectModifierTarget::ThrowHorizontalBoost, 0.0F) != 0.0F) {
        ApplyEffectHookToEntity(
            thrower,
            state,
            &audio,
            EffectHookContext{
                .type = EffectHookType::Throw,
                .actor_vid = thrower.vid,
                .target_vid = thrown.vid,
                .world_pos = thrown.GetCenter(),
            }
        );
    } else {
        RemoveEffect(thrown, EffectId::NoGravityUntilContact);
    }

    const Vec2 thrower_center = thrower.GetCenter();
    if (thrower.size.y <= thrown.size.y) {
        const float delta = std::abs(thrower.size.y - thrown.size.y) / 2.0F;
        thrown.SetCenter(thrower_center - Vec2::New(0.0F, delta));
    } else {
        thrown.SetCenter(thrower_center);
    }

    if (IsPlayerEntity(thrown, state)) {
        thrown.vel = throw_velocity;
        thrown.acc = Vec2::New(0.0F, 0.0F);
    } else {
        thrown.vel = Vec2::New(0.0F, 0.0F);
        thrown.acc = throw_velocity;
    }
    state.UpdateSidForEntity(thrown.vid.id, graphics);
    world_ops::MarkEntityThrown(state, thrower, thrown, throw_velocity);
    (void)PlayEntityCenterSoundEmitter(state, thrower, audio_asset_ids::Throw);
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
    std::vector<VID> changed_holders;
    if (entity.held_by_vid.has_value()) {
        if (Entity* const holder = state.entity_manager.GetEntityMut(*entity.held_by_vid)) {
            bool holder_changed = false;
            if (holder->holding_vid.has_value() && *holder->holding_vid == entity.vid) {
                holder->holding_vid.reset();
                holder->holding = false;
                holder->holding_timer = kDefaultHoldingTimer;
                holder_changed = true;
            }
            if (holder->back_vid.has_value() && *holder->back_vid == entity.vid) {
                holder->back_vid.reset();
                holder_changed = true;
            }
            if (holder_changed) {
                TrackChangedEntity(changed_holders, holder->vid);
            }
        }
    }

    for (Entity& candidate_holder : state.entity_manager.entities) {
        if (!candidate_holder.active || candidate_holder.vid == entity.vid) {
            continue;
        }
        bool holder_changed = false;
        if (candidate_holder.holding_vid.has_value() &&
            *candidate_holder.holding_vid == entity.vid) {
            candidate_holder.holding_vid.reset();
            candidate_holder.holding = false;
            candidate_holder.holding_timer = kDefaultHoldingTimer;
            holder_changed = true;
        }
        if (candidate_holder.back_vid.has_value() &&
            *candidate_holder.back_vid == entity.vid) {
            candidate_holder.back_vid.reset();
            holder_changed = true;
        }
        if (holder_changed) {
            TrackChangedEntity(changed_holders, candidate_holder.vid);
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

    for (const VID& holder_vid : changed_holders) {
        if (Entity* const holder = state.entity_manager.GetEntityMut(holder_vid)) {
            world_ops::PatchEntityState(state, entity, *holder);
        }
    }
}

void ReleaseEntityFromHolderAndEmitNetwork(Entity& entity, State& state) {
    if (!HasAttachmentReference(entity, state)) {
        return;
    }
    const std::optional<VID> dropped_by_vid = entity.held_by_vid;
    ReleaseEntityFromHolder(entity, state);
    world_ops::MarkEntityDropped(state, entity, dropped_by_vid);
}

std::vector<VID> SeverEntityCarryLinksForReset(Entity& entity, State& state) {
    std::vector<VID> changed_entities;

    const std::optional<VID> held_vid = entity.holding_vid;
    const std::optional<VID> back_vid = entity.back_vid;
    if (held_vid.has_value()) {
        if (Entity* const held = state.entity_manager.GetEntityMut(*held_vid)) {
            RestoreDetachedCarryEntity(*held);
            TrackChangedEntity(changed_entities, held->vid);
        }
    }
    if (back_vid.has_value()) {
        if (Entity* const back = state.entity_manager.GetEntityMut(*back_vid)) {
            RestoreDetachedCarryEntity(*back);
            TrackChangedEntity(changed_entities, back->vid);
        }
    }

    const std::optional<VID> holder_vid = entity.held_by_vid;
    if (holder_vid.has_value()) {
        if (Entity* const holder = state.entity_manager.GetEntityMut(*holder_vid)) {
            bool holder_changed = false;
            if (holder->holding_vid.has_value() && *holder->holding_vid == entity.vid) {
                holder->holding_vid.reset();
                holder->holding = false;
                holder->holding_timer = kDefaultHoldingTimer;
                holder_changed = true;
            }
            if (holder->back_vid.has_value() && *holder->back_vid == entity.vid) {
                holder->back_vid.reset();
                holder_changed = true;
            }
            if (holder_changed) {
                TrackChangedEntity(changed_entities, holder->vid);
            }
        }
    }

    // Repair asymmetric references too; resets may happen after one side has
    // already been respawned or corrected by a network state patch.
    for (Entity& candidate : state.entity_manager.entities) {
        if (!candidate.active || candidate.vid == entity.vid) {
            continue;
        }

        bool candidate_changed = false;
        if (candidate.holding_vid.has_value() && *candidate.holding_vid == entity.vid) {
            candidate.holding_vid.reset();
            candidate.holding = false;
            candidate.holding_timer = kDefaultHoldingTimer;
            candidate_changed = true;
        }
        if (candidate.back_vid.has_value() && *candidate.back_vid == entity.vid) {
            candidate.back_vid.reset();
            candidate_changed = true;
        }
        if (candidate.held_by_vid.has_value() && *candidate.held_by_vid == entity.vid) {
            RestoreDetachedCarryEntity(candidate);
            candidate_changed = true;
        }
        if (candidate_changed) {
            TrackChangedEntity(changed_entities, candidate.vid);
        }
    }

    entity.holding = false;
    entity.holding_vid.reset();
    entity.back_vid.reset();
    RestoreDetachedCarryEntity(entity);
    TrackChangedEntity(changed_entities, entity.vid);
    return changed_entities;
}

void DropHeldItemFromEntity(Entity& entity, State& state) {
    if (!entity.holding_vid.has_value()) {
        return;
    }

    Entity* const held = state.entity_manager.GetEntityMut(*entity.holding_vid);
    entity.holding_vid.reset();
    entity.holding = false;
    entity.holding_timer = kDefaultHoldingTimer;
    world_ops::PatchEntityState(state, entity, entity);
    if (held == nullptr) {
        return;
    }

    if (IsPlayerEntity(*held, state)) {
        const VID dropped_by_vid = entity.vid;
        ReleaseEntityFromHolder(*held, state);
        held->vel = Vec2::New(0.0F, 0.0F);
        held->acc = Vec2::New(0.0F, 0.0F);
        state.contact.AddInteractionCooldown(
            entity.vid,
            held->vid,
            InteractionCooldownKind::Harm,
            state.stage_frame,
            kForcedPlayerDropHarmCooldownFrames
        );
        state.contact.AddInteractionCooldown(
            held->vid,
            entity.vid,
            InteractionCooldownKind::Harm,
            state.stage_frame,
            kForcedPlayerDropHarmCooldownFrames
        );
        world_ops::MarkEntityDropped(state, *held, dropped_by_vid);
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
    world_ops::MarkEntityThrown(state, entity, *held, held->vel);
}

bool TryPickupEntityByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
) {
    Entity* const holder = state.entity_manager.GetEntityMut(holder_vid);
    Entity* const held = state.entity_manager.GetEntityMut(held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return false;
    }
    if (holder->holding_vid.has_value()) {
        return false;
    }
    if (!CanPickUpCandidate(*holder, *held, state)) {
        return false;
    }

    holder->holding_timer = kDefaultHoldingTimer;
    AttachEntityAsHeld(*holder, *held);
    SyncHeldAttachmentForHolder(*holder, state, graphics);
    world_ops::MarkEntityHeld(state, *holder, *held);
    return true;
}

bool TryDropEntityByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
) {
    Entity* const holder = state.entity_manager.GetEntityMut(holder_vid);
    Entity* const held = state.entity_manager.GetEntityMut(held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return false;
    }
    if ((!holder->holding_vid.has_value() || *holder->holding_vid != held_vid) &&
        (!held->held_by_vid.has_value() || *held->held_by_vid != holder_vid)) {
        return false;
    }

    ReleaseEntityFromHolder(*held, state);
    held->vel = Vec2::New(0.0F, 0.0F);
    held->acc = Vec2::New(0.0F, 0.0F);
    state.UpdateSidForEntity(held->vid.id, graphics);
    world_ops::MarkEntityDropped(state, *held, holder->vid);
    return true;
}

bool TryThrowEntityByVid(
    VID thrower_vid,
    VID thrown_vid,
    Vec2 throw_velocity,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    Entity* const thrower = state.entity_manager.GetEntityMut(thrower_vid);
    Entity* const thrown = state.entity_manager.GetEntityMut(thrown_vid);
    if (thrower == nullptr || thrown == nullptr || !thrower->active || !thrown->active) {
        return false;
    }
    if ((!thrower->holding_vid.has_value() || *thrower->holding_vid != thrown_vid) &&
        (!thrown->held_by_vid.has_value() || *thrown->held_by_vid != thrower_vid)) {
        return false;
    }

    ApplyThrowState(*thrower, *thrown, throw_velocity, state, graphics, audio);
    return true;
}

bool TryPutHeldEntityOnBackByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
) {
    Entity* const holder = state.entity_manager.GetEntityMut(holder_vid);
    Entity* const held = state.entity_manager.GetEntityMut(held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return false;
    }
    if (!holder->holding_vid.has_value() || *holder->holding_vid != held_vid ||
        holder->back_vid.has_value() || !held->can_go_on_back) {
        return false;
    }

    holder->back_vid = held->vid;
    holder->holding_vid.reset();
    holder->holding = false;
    holder->holding_timer = kDefaultHoldingTimer;

    ApplyHeldState(*held);
    held->held_by_vid = holder->vid;
    held->attachment_mode = AttachmentMode::Back;
    held->has_physics = false;
    held->can_collide = false;
    SyncEntityAttachments(holder->vid.id, state, graphics);
    world_ops::MarkEntityHeld(state, *holder, *held, AttachmentMode::Back);
    world_ops::PatchEntityState(state, *holder, *holder);
    world_ops::PatchEntityState(state, *holder, *held);
    return true;
}

bool TryTakeOffBackEntityByVid(
    VID holder_vid,
    VID back_vid,
    State& state,
    const Graphics& graphics
) {
    Entity* const holder = state.entity_manager.GetEntityMut(holder_vid);
    Entity* const back_item = state.entity_manager.GetEntityMut(back_vid);
    if (holder == nullptr || back_item == nullptr || !holder->active || !back_item->active) {
        return false;
    }
    if (!holder->back_vid.has_value() || *holder->back_vid != back_vid) {
        return false;
    }

    back_item->has_physics = true;
    back_item->can_collide = true;
    TrySetAnimation(*back_item, EntityDisplayState::Neutral);
    back_item->held_by_vid.reset();
    back_item->attachment_mode = AttachmentMode::None;
    StopUsingEntity(*back_item);
    back_item->thrown_by = holder->vid;
    back_item->thrown_immunity_timer = kThrownByImmunityDuration;
    const EntityArchetype& back_item_archetype = GetEntityArchetype(back_item->type_);
    back_item->can_apply_projectile_contact = back_item_archetype.can_apply_projectile_contact;
    back_item->projectile_contact_damage_type = back_item_archetype.projectile_contact_damage_type;
    back_item->projectile_contact_damage_amount = back_item_archetype.projectile_contact_damage_amount;
    back_item->projectile_contact_timer = kProjectileContactDuration;
    RemoveEffect(*back_item, EffectId::NoGravityUntilContact);

    holder->back_vid.reset();
    state.UpdateSidForEntity(back_item->vid.id, graphics);
    world_ops::PatchEntityState(state, *holder, *holder);
    world_ops::PatchEntityState(state, *holder, *back_item);
    return true;
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
                        entity.holding_timer = kDefaultHoldingTimer;
                        if (!ShouldRequestCarryAction(state, entity)) {
                            entity.holding_vid.reset();
                            entity.holding = false;
                        }
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
                if (ShouldRequestCarryAction(state, entity)) {
                    EmitCarryActionRequest(
                        state,
                        GameplayActionKind::PickupEntity,
                        entity.vid,
                        trying_to_pick_this_up_vid
                    );
                } else {
                    (void)TryPickupEntityByVid(
                        entity.vid,
                        *trying_to_pick_this_up_vid,
                        state,
                        graphics
                    );
                }
            }
        }

        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            const bool trying_to_go_down = control.down;
            const bool trying_to_go_up = control.up;
            const bool trying_to_go_left = control.left;
            const bool trying_to_go_right = control.right;
            const float mitt_throw_boost =
                entity.grounded && trying_to_go_down
                    ? 0.0F
                    : GetModifiedEffectValue(entity, EffectModifierTarget::ThrowHorizontalBoost, 0.0F);
            const bool mitt_throw = mitt_throw_boost != 0.0F;

            if (thrown_vid.has_value()) {
                if (Entity* const thrown = state.entity_manager.GetEntityMut(*thrown_vid)) {
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
                    }

                    const Vec2 scaled_throw_vel = throw_vel * entity.throw_velocity_scale;
                    if (ShouldRequestCarryAction(state, entity)) {
                        EmitCarryActionRequest(
                            state,
                            GameplayActionKind::ThrowEntity,
                            entity.vid,
                            thrown->vid,
                            scaled_throw_vel
                        );
                    } else {
                        (void)TryThrowEntityByVid(
                            entity.vid,
                            thrown->vid,
                            scaled_throw_vel,
                            state,
                            graphics,
                            audio
                        );
                    }
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
            const Entity& entity = state.entity_manager.entities[entity_idx];
            if (ShouldRequestCarryAction(state, entity)) {
                EmitCarryActionRequest(
                    state,
                    GameplayActionKind::TakeOffBackEntity,
                    entity.vid,
                    take_off_back_vid
                );
            } else {
                (void)TryTakeOffBackEntityByVid(entity_vid, *take_off_back_vid, state, graphics);
            }
        }

        {
            Entity& entity = state.entity_manager.entities[entity_idx];
            if (put_held_on_back) {
                const std::optional<VID> held_vid = entity.holding_vid;
                if (held_vid.has_value() && ShouldRequestCarryAction(state, entity)) {
                    EmitCarryActionRequest(
                        state,
                        GameplayActionKind::PutHeldEntityOnBack,
                        entity.vid,
                        held_vid
                    );
                } else if (held_vid.has_value()) {
                    (void)TryPutHeldEntityOnBackByVid(entity.vid, *held_vid, state, graphics);
                }
            }
        }
    }

    {
        const Entity& entity = state.entity_manager.entities[entity_idx];
        if (entity.holding_vid.has_value()) {
            if (Entity* const holding = state.entity_manager.GetEntityMut(*entity.holding_vid)) {
                if (ShouldRequestCarryAction(state, entity)) {
                    if (control.use_pressed || control.use_released) {
                        EmitAttachmentUseActionRequest(
                            state,
                            GameplayActionKind::UseHeldEntity,
                            entity.vid,
                            holding->vid,
                            control.use_held,
                            control
                        );
                    }
                } else if (control.use_held) {
                    UseEntity(*holding, entity.vid, AttachmentMode::Held);
                } else {
                    StopUsingEntity(*holding);
                }
            }
        }
        if (entity.back_vid.has_value()) {
            if (Entity* const back_item = state.entity_manager.GetEntityMut(*entity.back_vid)) {
                if (ShouldRequestCarryAction(state, entity)) {
                    if (control.use_back_pressed || control.use_back_released) {
                        EmitAttachmentUseActionRequest(
                            state,
                            GameplayActionKind::UseBackEntity,
                            entity.vid,
                            back_item->vid,
                            control.use_back,
                            control
                        );
                    }
                } else if (control.use_back) {
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

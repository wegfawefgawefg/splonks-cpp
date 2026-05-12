#include "network/net_message_apply_internal.hpp"

#include "audio_emitters.hpp"
#include "network/net_entity_interpolation.hpp"
#include "network/net_session.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entity/display_states.hpp"
#include "entity/replicated_runtime_flags.hpp"
#include "effects.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "on_damage_effects.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace splonks::network {

constexpr std::uint32_t kForcedPlayerDropHarmCooldownFrames = 12;

std::uint32_t AddSignedClamped(std::uint32_t value, std::int32_t delta) {
    const std::int64_t next = static_cast<std::int64_t>(value) + static_cast<std::int64_t>(delta);
    const std::int64_t clamped =
        std::clamp<std::int64_t>(next, 0, std::numeric_limits<std::uint32_t>::max());
    return static_cast<std::uint32_t>(clamped);
}

std::optional<VID> FindEntityVidForMessage(
    NetSessionState& session,
    State& state,
    NetEntityId entity_id
);

bool IsAttachmentDriven(const Entity& entity) {
    return entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None;
}

bool IsEntityManagerVid(const State& state, VID vid) {
    return state.entity_manager.GetEntity(vid) != nullptr;
}

void ApplyEntityLinkSlot(
    NetSessionState& session,
    State& state,
    Entity& entity,
    const EntityStatePatchedMessage& payload
) {
    if (payload.entity_a_id != kInvalidNetEntityId) {
        const std::optional<VID> linked_vid =
            FindEntityVidForMessage(session, state, payload.entity_a_id);
        if (linked_vid.has_value()) {
            entity.entity_a = *linked_vid;
            entity.point_a = payload.point_a;
        }
        return;
    }

    if (entity.entity_a.has_value() && IsEntityManagerVid(state, *entity.entity_a)) {
        entity.entity_a.reset();
        entity.point_a = payload.point_a;
    }
}

void ApplyOptionalEntityLinkSlot(
    NetSessionState& session,
    State& state,
    std::optional<VID>& slot,
    NetEntityId entity_id
) {
    if (entity_id == kInvalidNetEntityId) {
        slot.reset();
        return;
    }
    if (const std::optional<VID> resolved = FindEntityVidForMessage(session, state, entity_id)) {
        slot = *resolved;
    }
}

void ApplyEntityScratchLinks(
    NetSessionState& session,
    State& state,
    Entity& entity,
    const EntityStatePatchedMessage& payload
) {
    ApplyOptionalEntityLinkSlot(session, state, entity.entity_b, payload.entity_b_id);
    ApplyOptionalEntityLinkSlot(session, state, entity.entity_c, payload.entity_c_id);
    ApplyOptionalEntityLinkSlot(session, state, entity.entity_d, payload.entity_d_id);
}

bool ApplyConditionPresentation(Entity& entity, EntityCondition condition) {
    std::optional<EntityDisplayState> display_state;
    if (condition == EntityCondition::Dead) {
        display_state = EntityDisplayState::Dead;
    } else if (condition == EntityCondition::Stunned) {
        display_state = EntityDisplayState::Stunned;
    }
    if (!display_state.has_value()) {
        return false;
    }

    const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
        .type_ = entity.type_,
        .display_state = *display_state,
    });
    if (!selection.has_value()) {
        return false;
    }

    FrameDataAnimator& animator = entity.frame_data_animator;
    if (animator.animation_id != selection->animation_id) {
        animator.SetAnimation(selection->animation_id);
    }
    animator.animate = selection->animate;
    animator.current_frame = selection->has_forced_frame ? selection->forced_frame : 0;
    animator.current_time = 0.0F;
    animator.speed = 1.0F;
    return true;
}

void ApplyDamagePresentation(Entity& entity, const EntityDamagedMessage& payload) {
    const EntityCondition effective_condition =
        payload.remaining_health == 0
            ? EntityCondition::Dead
            : static_cast<EntityCondition>(payload.condition);
    if (ApplyConditionPresentation(entity, effective_condition)) {
        return;
    }
    if (payload.animation_id == kInvalidFrameDataId) {
        return;
    }

    FrameDataAnimator& animator = entity.frame_data_animator;
    if (animator.animation_id != payload.animation_id) {
        animator.SetAnimation(payload.animation_id);
    }
    animator.current_frame = payload.animation_frame;
    animator.current_time = payload.animation_time;
    animator.speed = payload.animation_speed;
    animator.animate = payload.animate != 0;
}

void ApplyStatePatchPresentation(Entity& entity, const EntityStatePatchedMessage& payload) {
    if (payload.animation_id == kInvalidFrameDataId) {
        return;
    }

    FrameDataAnimator& animator = entity.frame_data_animator;
    if (animator.animation_id != payload.animation_id) {
        animator.SetAnimation(payload.animation_id);
    }
    animator.current_frame = payload.animation_frame;
    animator.current_time = payload.animation_time;
    animator.speed = payload.animation_speed;
    animator.animate = payload.animate != 0;
    animator.loop = payload.animation_loop != 0;
    animator.finished = payload.animation_finished != 0;
}

void ApplySpawnPresentation(Entity& entity, const EntitySpawnedMessage& payload) {
    if (payload.animation_id == kInvalidFrameDataId) {
        return;
    }

    FrameDataAnimator& animator = entity.frame_data_animator;
    if (animator.animation_id != payload.animation_id) {
        animator.SetAnimation(payload.animation_id);
    }
    animator.current_frame = payload.animation_frame;
    animator.current_time = payload.animation_time;
    animator.speed = payload.animation_speed;
    animator.animate = payload.animate != 0;
    animator.loop = payload.animation_loop != 0;
    animator.finished = payload.animation_finished != 0;
}

template <typename Payload>
void ApplyReplicatedEffects(Entity& entity, const Payload& payload) {
    entity.effects.reset();
    const std::size_t effect_count =
        std::min<std::size_t>(payload.effect_count, payload.effects.size());
    if (effect_count == 0) {
        return;
    }

    EntityEffects& effects = entity.effects.emplace();
    effects.count = static_cast<std::uint8_t>(effect_count);
    for (std::size_t i = 0; i < effect_count; ++i) {
        const EntityReplicatedEffect& effect = payload.effects[i];
        effects.effects[i] = EffectInstance{
            .id = effect.id,
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
}

void ApplyEntitySpawnedMessage(
    NetSessionState& session,
    State& state,
    const EntitySpawnedMessage& payload,
    Graphics* graphics
) {
    if (payload.entity_id == kInvalidNetEntityId || payload.entity_type == EntityType::None) {
        return;
    }

    if (session.FindLocalVid(payload.entity_id).has_value()) {
        return;
    }

    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, payload.entity_type);
    entity->pos = payload.pos;
    entity->vel = payload.vel;
    entity->acc = payload.acc;
    entity->size = payload.size;
    entity->counter_a = payload.counter_a;
    entity->counter_b = payload.counter_b;
    entity->light_strength = payload.light_strength;
    entity->light_color = payload.light_color;
    entity->light_radius = payload.light_radius;
    entity->movement_flags = payload.movement_flags;
    ApplyReplicatedEffects(*entity, payload);
    ApplySpawnPresentation(*entity, payload);
    std::optional<VID> holder_to_sync;
    if (payload.held_by_id != kInvalidNetEntityId) {
        if (const std::optional<VID> holder_vid =
                FindEntityVidForMessage(session, state, payload.held_by_id)) {
            Entity* const holder = state.entity_manager.GetEntityMut(*holder_vid);
            if (holder != nullptr && holder->active) {
                holder->holding_vid = entity->vid;
                holder->holding = true;
                entity->held_by_vid = holder->vid;
                entity->attachment_mode = AttachmentMode::Held;
                entity->has_physics = false;
                entity->can_collide = false;
                holder_to_sync = holder->vid;
            }
        }
    }
    if (payload.use_pressed) {
        UseEntity(*entity, std::nullopt, AttachmentMode::None);
    }
    session.LinkEntity(payload.entity_id, entity->vid);
    session.SetEntityOwner(payload.entity_id, payload.owner.player_id);
    if (graphics != nullptr) {
        if (holder_to_sync.has_value()) {
            entities::common::SyncEntityAttachments(holder_to_sync->id, state, *graphics);
        }
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
}

void ApplyEntityDeactivatedMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityIdMessage& payload
) {
    const std::optional<VID> vid = FindEntityVidForMessage(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    ClearRemoteEntityRenderTarget(state, payload.entity_id);
    state.entity_manager.SetInactive(vid->id);
    if (graphics != nullptr) {
        state.UpdateSidForEntity(vid->id, *graphics);
    }
}

std::optional<VID> FindEntityVidForMessage(
    NetSessionState& session,
    State& state,
    NetEntityId entity_id
) {
    if (const std::optional<VID> linked = session.FindLocalVid(entity_id)) {
        return linked;
    }
    if (IsPlayerNetEntityId(entity_id)) {
        const PlayerId player_id = GetPlayerIdFromNetEntityId(entity_id);
        const PlayerSlot* const slot = state.players.Find(player_id);
        if (slot == nullptr || !slot->entity_vid.has_value()) {
            return std::nullopt;
        }
        if (const Entity* const entity = state.entity_manager.GetEntity(*slot->entity_vid)) {
            if (!entity->active) {
                return std::nullopt;
            }
        }
        session.LinkEntity(entity_id, *slot->entity_vid);
        return slot->entity_vid;
    }
    return std::nullopt;
}

void ApplyEntityDamagedMessage(
    NetSessionState& session,
    State& state,
    Audio* audio,
    PlayerId source_player_id,
    const EntityDamagedMessage& payload
) {
    (void)source_player_id;
    const std::optional<VID> vid = FindEntityVidForMessage(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    std::optional<VID> source_vid;
    const Entity* source_entity = nullptr;
    if (payload.source_entity_id != kInvalidNetEntityId) {
        source_vid = FindEntityVidForMessage(session, state, payload.source_entity_id);
        if (source_vid.has_value()) {
            source_entity = state.entity_manager.GetEntity(*source_vid);
        }
    }
    if (source_entity != nullptr &&
        IsPlayerLikeEntityType(source_entity->type_) &&
        IsPlayerLikeEntityType(entity->type_) &&
        (payload.damage_type == DamageType::Attack ||
         payload.damage_type == DamageType::JumpOn)) {
        return;
    }

    const bool attachment_driven = IsAttachmentDriven(*entity);
    if (!attachment_driven) {
        entity->pos = payload.pos;
        entity->vel = payload.vel;
        entity->acc = payload.acc;
        entity->grounded = payload.grounded != 0;
    }
    entity->fall_timer = payload.fall_timer;
    entity->stun_timer = payload.stun_timer;
    entity->projectile_contact_timer = payload.projectile_contact_timer;

    if (payload.remaining_health == 0) {
        entity->health = 0;
        if (audio != nullptr) {
            entities::common::DieIfDead(entity->vid.id, state, *audio);
        } else {
            entity->condition = EntityCondition::Dead;
        }
        ApplyDamagePresentation(*entity, payload);
        return;
    }

    entity->health = payload.remaining_health;
    entity->condition = static_cast<EntityCondition>(payload.condition);
    ApplyDamagePresentation(*entity, payload);
    if (entity->condition == EntityCondition::Stunned ||
        entity->condition == EntityCondition::Dead) {
        entities::common::ReleaseEntityFromHolder(*entity, state);
    }
    if (audio != nullptr && payload.amount > 0) {
        if (entity->damage_animation.has_value()) {
            SpawnDamageEffectAnimationBurst(*entity->damage_animation, entity->GetCenter(), state);
        }
        if (entity->damage_sound.has_value()) {
            (void)PlayEntityCenterSoundEmitter(state, *entity, *entity->damage_sound);
        }
    }
}

void ApplyEntityStatePatchedMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    PlayerId source_player_id,
    const EntityStatePatchedMessage& payload
) {
    const bool from_coordinator = source_player_id == session.coordinator_player_id;
    const bool has_explicit_source = payload.source_entity_id != kInvalidNetEntityId;
    if (!from_coordinator) {
        if (has_explicit_source) {
            const std::optional<PlayerId> source_owner =
                session.FindEntityOwner(payload.source_entity_id);
            if (!source_owner.has_value() || *source_owner != source_player_id) {
                return;
            }
        } else if (const std::optional<PlayerId> owner_player_id =
                       session.FindEntityOwner(payload.entity_id)) {
            if (*owner_player_id == session.local_player_id ||
                *owner_player_id != source_player_id) {
                return;
            }
        } else {
            return;
        }
    }

    const std::optional<VID> vid = FindEntityVidForMessage(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }
    const PlayerSlot* const player_slot = state.players.FindByEntityVid(entity->vid);
    const bool target_is_local_player =
        player_slot != nullptr && player_slot->connection_kind == PlayerConnectionKind::Local;
    const bool preserve_local_player_motion =
        from_coordinator &&
        target_is_local_player &&
        !has_explicit_source;
    ApplyEntityLinkSlot(session, state, *entity, payload);
    ApplyEntityScratchLinks(session, state, *entity, payload);
    ApplyOptionalEntityLinkSlot(session, state, entity->holding_vid, payload.holding_id);
    ApplyOptionalEntityLinkSlot(session, state, entity->held_by_vid, payload.held_by_id);
    ApplyOptionalEntityLinkSlot(session, state, entity->back_vid, payload.back_id);
    entity->attachment_mode = static_cast<AttachmentMode>(payload.attachment_mode);
    if (payload.active == 0) {
        ClearRemoteEntityRenderTarget(state, payload.entity_id);
        state.entity_manager.SetInactive(entity->vid.id);
        return;
    }
    const bool attachment_driven = IsAttachmentDriven(*entity);
    if (attachment_driven) {
        ClearRemoteEntityRenderTarget(state, payload.entity_id);
    }
    const bool target_is_player = player_slot != nullptr;
    const bool target_has_local_authority = state.net_session.HasLocalAuthorityForEntity(entity->vid);
    const bool should_interpolate_render =
        from_coordinator &&
        !target_is_player &&
        !target_has_local_authority &&
        !attachment_driven &&
        !preserve_local_player_motion;
    std::optional<Vec2> previous_render_pos = std::nullopt;
    if (should_interpolate_render) {
        previous_render_pos = GetRemoteEntityRenderPosition(state, *entity);
        if (!previous_render_pos.has_value()) {
            previous_render_pos = entity->pos;
        }
    }
    if (!attachment_driven && !preserve_local_player_motion) {
        entity->pos = payload.pos;
        entity->vel = payload.vel;
        entity->acc = payload.acc;
    }
    entity->grounded = payload.grounded != 0;
    entity->size = payload.size;
    entity->counter_a = payload.counter_a;
    entity->counter_b = payload.counter_b;
    entity->counter_c = payload.counter_c;
    entity->counter_d = payload.counter_d;
    entity->threshold_a = payload.threshold_a;
    entity->threshold_b = payload.threshold_b;
    entity->point_a = payload.point_a;
    entity->point_b = payload.point_b;
    entity->point_c = payload.point_c;
    entity->point_d = payload.point_d;
    entity->has_physics = payload.has_physics != 0;
    entity->can_collide = payload.can_collide != 0;
    entity->can_apply_projectile_contact = payload.can_apply_projectile_contact != 0;
    entity->damage_vulnerability =
        static_cast<DamageVulnerability>(payload.damage_vulnerability);
    entity->projectile_contact_timer = payload.projectile_contact_timer;
    entity->light_strength = payload.light_strength;
    entity->light_color = payload.light_color;
    entity->light_radius = payload.light_radius;
    entity->rotation = payload.rotation;
    entity->facing = payload.facing != 0 ? LeftOrRight::Right : LeftOrRight::Left;
    entity->ai_state = static_cast<EntityAiState>(payload.ai_state);
    entity->wanted = payload.wanted != 0;
    entity->holding = payload.holding != 0;
    entity->render_enabled = payload.render_enabled != 0;
    entity->draw_layer = static_cast<DrawLayer>(payload.draw_layer);
    entity->movement_flags = payload.movement_flags;
    entity->money = payload.money;
    entity->stage_exit_id = payload.stage_exit_id;
    ApplyReplicatedRuntimeFlags(*entity, payload.runtime_flags);
    ApplyReplicatedEffects(*entity, payload);
    entity->buyable.active = payload.buyable_active != 0;
    entity->buyable.display_quantity = payload.buyable_display_quantity;
    entity->buyable.display_icon_animation_id =
        payload.buyable_display_icon_animation_id != kInvalidFrameDataId
            ? std::optional<FrameDataId>(payload.buyable_display_icon_animation_id)
            : std::nullopt;
    ApplyOptionalEntityLinkSlot(
        session,
        state,
        entity->buyable.shop_owner_vid,
        payload.buyable_shop_owner_id
    );
    entity->health = payload.health;
    entity->coyote_time = payload.coyote_time;
    entity->fall_timer = payload.fall_timer;
    entity->stun_timer = payload.stun_timer;
    entity->condition = static_cast<EntityCondition>(payload.condition);
    ApplyStatePatchPresentation(*entity, payload);
    state.stage.NormalizeEntityPositionForWrap(*entity);
    if (should_interpolate_render && previous_render_pos.has_value()) {
        SetRemoteEntityRenderTarget(
            state,
            payload.entity_id,
            *previous_render_pos,
            entity->pos
        );
    }
    if (graphics != nullptr) {
        if (entity->holding_vid.has_value() || entity->back_vid.has_value()) {
            entities::common::SyncEntityAttachments(entity->vid.id, state, *graphics);
        }
        if (attachment_driven && entity->held_by_vid.has_value()) {
            entities::common::SyncEntityAttachments(entity->held_by_vid->id, state, *graphics);
        }
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
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

void ApplyEntityHeldMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityHeldMessage& payload
) {
    const std::optional<VID> holder_vid = FindEntityVidForMessage(session, state, payload.holder_id);
    const std::optional<VID> held_vid = FindEntityVidForMessage(session, state, payload.held_id);
    if (!holder_vid.has_value() || !held_vid.has_value() || *holder_vid == *held_vid) {
        return;
    }
    Entity* const holder = state.entity_manager.GetEntityMut(*holder_vid);
    Entity* const held = state.entity_manager.GetEntityMut(*held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return;
    }
    if (held->held_by_vid.has_value() && *held->held_by_vid != holder->vid) {
        return;
    }
    if (held->attachment_mode != AttachmentMode::None &&
        (!held->held_by_vid.has_value() || *held->held_by_vid != holder->vid)) {
        return;
    }
    if (IsVidInHolderChain(held->vid, *holder, state)) {
        return;
    }

    if (payload.attachment_mode == AttachmentMode::Back) {
        if (holder->back_vid.has_value() && *holder->back_vid != held->vid) {
            return;
        }
        if (holder->holding_vid.has_value() && *holder->holding_vid != held->vid) {
            return;
        }

        holder->back_vid = held->vid;
        if (holder->holding_vid.has_value() && *holder->holding_vid == held->vid) {
            holder->holding_vid.reset();
            holder->holding = false;
            holder->holding_timer = kDefaultHoldingTimer;
        }
        held->held_by_vid = holder->vid;
        held->attachment_mode = AttachmentMode::Back;
        held->has_physics = false;
        held->can_collide = false;
    } else {
        if (holder->holding_vid.has_value() && *holder->holding_vid != held->vid) {
            return;
        }
        entities::common::AttachEntityAsHeld(*holder, *held);
    }
    ClearRemoteEntityRenderTarget(state, payload.held_id);
    if (graphics != nullptr) {
        entities::common::SyncEntityAttachments(holder->vid.id, state, *graphics);
        state.UpdateSidForEntity(holder->vid.id, *graphics);
        state.UpdateSidForEntity(held->vid.id, *graphics);
    }
}

void ApplyEntityDroppedMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityDroppedMessage& payload
) {
    const std::optional<VID> vid = FindEntityVidForMessage(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr || !entity->active) {
        return;
    }

    const std::optional<VID> dropped_by_vid =
        payload.dropped_by_id != kInvalidNetEntityId
            ? FindEntityVidForMessage(session, state, payload.dropped_by_id)
            : std::nullopt;
    ClearRemoteEntityRenderTarget(state, payload.entity_id);
    entities::common::ReleaseEntityFromHolder(*entity, state);
    if (dropped_by_vid.has_value()) {
        state.contact.AddInteractionCooldown(
            *dropped_by_vid,
            entity->vid,
            InteractionCooldownKind::Harm,
            state.stage_frame,
            kForcedPlayerDropHarmCooldownFrames
        );
        state.contact.AddInteractionCooldown(
            entity->vid,
            *dropped_by_vid,
            InteractionCooldownKind::Harm,
            state.stage_frame,
            kForcedPlayerDropHarmCooldownFrames
        );
    }
    if (IsPlayerNetEntityId(payload.entity_id)) {
        const PlayerId player_id = GetPlayerIdFromNetEntityId(payload.entity_id);
        const PlayerSlot* const slot = state.players.Find(player_id);
        if (slot != nullptr && slot->connection_kind == PlayerConnectionKind::Local) {
            if (graphics != nullptr) {
                state.UpdateSidForEntity(entity->vid.id, *graphics);
            }
            return;
        }
    }
    entity->pos = payload.pos;
    entity->vel = payload.vel;
    entity->acc = Vec2::New(0.0F, 0.0F);
    entity->grounded = false;
    if (graphics != nullptr) {
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
}

void ApplyEntityThrownMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityThrownMessage& payload
) {
    const std::optional<VID> vid = FindEntityVidForMessage(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    if (!IsPlayerNetEntityId(payload.entity_id)) {
        session.SetEntityOwner(payload.entity_id, std::nullopt);
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr || !entity->active) {
        return;
    }

    std::optional<VID> thrower_vid;
    if (payload.thrower_id != kInvalidNetEntityId) {
        thrower_vid = FindEntityVidForMessage(session, state, payload.thrower_id);
    }

    entities::common::ReleaseEntityFromHolder(*entity, state);
    ClearRemoteEntityRenderTarget(state, payload.entity_id);
    entity->pos = payload.pos;
    if (state.players.FindByEntityVid(entity->vid) != nullptr) {
        entity->vel = payload.vel;
        entity->acc = Vec2::New(0.0F, 0.0F);
    } else {
        entity->vel = Vec2::New(0.0F, 0.0F);
        entity->acc = payload.vel;
    }
    entity->thrown_by = thrower_vid;
    entity->thrown_immunity_timer = entities::common::kThrownByImmunityDuration;
    const EntityArchetype& archetype = GetEntityArchetype(entity->type_);
    entity->can_apply_projectile_contact = archetype.can_apply_projectile_contact;
    entity->projectile_contact_damage_type = archetype.projectile_contact_damage_type;
    entity->projectile_contact_damage_amount = archetype.projectile_contact_damage_amount;
    entity->projectile_contact_timer = entities::common::kProjectileContactDuration;
    entity->grounded = false;
    if (graphics != nullptr) {
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
}

} // namespace splonks::network

#include "network/net_event_apply.hpp"

#include "audio_emitters.hpp"
#include "network/net_event_apply_internal.hpp"
#include "network/net_event.hpp"
#include "network/net_session.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entity/display_states.hpp"
#include "entity/replicated_runtime_flags.hpp"
#include "entity_tool_inventory.hpp"
#include "effects.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "on_damage_effects.hpp"
#include "presentation_commands.hpp"
#include "stage_break.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

constexpr std::uint32_t kForcedPlayerDropHarmCooldownFrames = 12;

std::uint32_t AddSignedClamped(std::uint32_t value, std::int32_t delta) {
    const std::int64_t next = static_cast<std::int64_t>(value) + static_cast<std::int64_t>(delta);
    const std::int64_t clamped =
        std::clamp<std::int64_t>(next, 0, std::numeric_limits<std::uint32_t>::max());
    return static_cast<std::uint32_t>(clamped);
}

std::optional<VID> FindEntityVidForEvent(
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
    const EntityStatePatchedEvent& payload
) {
    if (payload.entity_a_id != kInvalidNetEntityId) {
        const std::optional<VID> linked_vid =
            FindEntityVidForEvent(session, state, payload.entity_a_id);
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
    if (const std::optional<VID> resolved = FindEntityVidForEvent(session, state, entity_id)) {
        slot = *resolved;
    }
}

void ApplyEntityScratchLinks(
    NetSessionState& session,
    State& state,
    Entity& entity,
    const EntityStatePatchedEvent& payload
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

void ApplyDamagePresentation(Entity& entity, const EntityDamagedEvent& payload) {
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

void ApplyStatePatchPresentation(Entity& entity, const EntityStatePatchedEvent& payload) {
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

void ApplySpawnPresentation(Entity& entity, const EntitySpawnedEvent& payload) {
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

void ApplyEntitySpawnedEvent(
    NetSessionState& session,
    State& state,
    const EntitySpawnedEvent& payload,
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
    entity->movement_flags = payload.movement_flags;
    ApplyReplicatedEffects(*entity, payload);
    ApplySpawnPresentation(*entity, payload);
    std::optional<VID> holder_to_sync;
    if (payload.held_by_id != kInvalidNetEntityId) {
        if (const std::optional<VID> holder_vid =
                FindEntityVidForEvent(session, state, payload.held_by_id)) {
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

void ApplyEntityDeactivatedEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityIdEvent& payload
) {
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    state.entity_manager.SetInactive(vid->id);
    if (graphics != nullptr) {
        state.UpdateSidForEntity(vid->id, *graphics);
    }
}

std::optional<VID> FindEntityVidForEvent(
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

void ApplyEntityDamagedEvent(
    NetSessionState& session,
    State& state,
    Audio* audio,
    PlayerId source_player_id,
    const EntityDamagedEvent& payload
) {
    (void)source_player_id;
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
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
        source_vid = FindEntityVidForEvent(session, state, payload.source_entity_id);
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

void ApplyEntityStatePatchedEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    PlayerId source_player_id,
    const EntityStatePatchedEvent& payload
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

    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
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
        state.entity_manager.SetInactive(entity->vid.id);
        return;
    }
    const bool attachment_driven = IsAttachmentDriven(*entity);
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

bool IsImmediateLocalNetResult(NetEventType type) {
    switch (type) {
    case NetEventType::EntitySpawned:
    case NetEventType::EntityDeactivated:
    case NetEventType::TileBroken:
    case NetEventType::TileChanged:
    case NetEventType::FluidCellPatched:
    case NetEventType::PlayerStatePatched:
    case NetEventType::PresentationCommand:
        return true;
    default:
        return false;
    }
}

bool IsTransientStateRepairEvent(const NetEvent& event) {
    if (event.type == NetEventType::FluidCellPatched) {
        return true;
    }
    if (event.type != NetEventType::EntityStatePatched) {
        return false;
    }
    const auto* const payload = std::get_if<EntityStatePatchedEvent>(&event.payload);
    return payload == nullptr || payload->source_entity_id == kInvalidNetEntityId;
}

bool ShouldSkipImmediateLocalApply(const NetSessionState& session, const NetEvent& event) {
    if (event.header.source_player_id != session.local_player_id ||
        !IsImmediateLocalNetResult(event.type)) {
        return false;
    }
    if (event.type == NetEventType::EntityStatePatched) {
        const auto* payload = std::get_if<EntityStatePatchedEvent>(&event.payload);
        return payload == nullptr || payload->source_entity_id == kInvalidNetEntityId;
    }
    return true;
}

void NoteAppliedCoordinatorOrder(NetSessionState& session, const NetEvent& event) {
    session.MarkCoordinatorOrderApplied(event);
}

void ApplyEntityHeldEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityHeldEvent& payload
) {
    const std::optional<VID> holder_vid = FindEntityVidForEvent(session, state, payload.holder_id);
    const std::optional<VID> held_vid = FindEntityVidForEvent(session, state, payload.held_id);
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
    if (graphics != nullptr) {
        entities::common::SyncEntityAttachments(holder->vid.id, state, *graphics);
        state.UpdateSidForEntity(holder->vid.id, *graphics);
        state.UpdateSidForEntity(held->vid.id, *graphics);
    }
}

void ApplyEntityDroppedEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityDroppedEvent& payload
) {
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr || !entity->active) {
        return;
    }

    const std::optional<VID> dropped_by_vid =
        payload.dropped_by_id != kInvalidNetEntityId
            ? FindEntityVidForEvent(session, state, payload.dropped_by_id)
            : std::nullopt;
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

void ApplyEntityThrownEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityThrownEvent& payload
) {
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
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
        thrower_vid = FindEntityVidForEvent(session, state, payload.thrower_id);
    }

    entities::common::ReleaseEntityFromHolder(*entity, state);
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

void ApplyPresentationCommandEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const PresentationCommandEvent& payload
) {
    if (graphics == nullptr) {
        return;
    }

    PresentationCommand command{
        .kind = static_cast<PresentationCommandKind>(payload.kind),
        .effect_id = static_cast<ScriptedPresentationEffectId>(payload.effect_id),
        .audio_asset_id = payload.audio_asset_id,
        .source_vid = payload.source_entity_id != kInvalidNetEntityId
            ? FindEntityVidForEvent(session, state, payload.source_entity_id)
            : std::nullopt,
        .target_vid = payload.target_entity_id != kInvalidNetEntityId
            ? FindEntityVidForEvent(session, state, payload.target_entity_id)
            : std::nullopt,
        .source_pos = payload.source_pos,
        .target_pos = payload.target_pos,
        .direction = IVec2::New(payload.direction_x, payload.direction_y),
        .entity_shake_amount = payload.entity_shake_amount,
        .foreground_shake_amount = payload.foreground_shake_amount,
        .background_shake_amount = payload.background_shake_amount,
        .area_entity_shake_amount = payload.area_entity_shake_amount,
        .shake_radius_tiles = payload.shake_radius_tiles,
    };
    PlayPresentationCommand(state, *graphics, command);
}

void ApplyPlayerStatePatchedEvent(
    NetSessionState& session,
    State& state,
    const PlayerStatePatchedEvent& payload
) {
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.player_entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const player = state.entity_manager.GetEntityMut(*vid);
    if (player == nullptr || !player->active) {
        return;
    }
    PlayerSlot* player_slot = state.players.FindByEntityVid(player->vid);
    if (player_slot == nullptr && payload.player_id != kInvalidPlayerId) {
        const bool local_player = payload.player_id == state.net_session.local_player_id;
        if (local_player) {
            player_slot = &state.players.EnsureLocalPlayer(payload.player_id, "Player", true);
        } else {
            player_slot = &state.players.EnsureRemotePlayer(
                payload.player_id,
                "Player " + std::to_string(payload.player_id)
            );
        }
        player_slot->entity_vid = player->vid;
    }
    if (player_slot == nullptr) {
        return;
    }
    if (player_slot->connection_kind == PlayerConnectionKind::Remote) {
        player_slot->connected = payload.connected != 0;
    }

    player->health = payload.health;
    player->money = payload.money;
    player->wanted = payload.wanted != 0;

    for (std::size_t i = 0; i < payload.tool_slots.size(); ++i) {
        ToolSlot& slot = state.entity_tools.EnsureToolSlot(player->vid, i);
        const PlayerStatePatchedToolSlot& patch = payload.tool_slots[i];
        slot.kind = patch.kind;
        slot.count = patch.count;
        slot.cooldown = patch.cooldown;
        slot.active = patch.active != 0;
    }

    player->effects.reset();
    const std::size_t effect_count =
        std::min<std::size_t>(payload.effect_count, payload.effects.size());
    if (effect_count > 0) {
        EntityEffects& effects = player->effects.emplace();
        effects.count = static_cast<std::uint8_t>(effect_count);
        for (std::size_t i = 0; i < effect_count; ++i) {
            const PlayerStatePatchedEffect& patch = payload.effects[i];
            effects.effects[i] = EffectInstance{
                .id = patch.id,
                .count = patch.count,
                .value = patch.value,
                .frames_remaining = patch.frames_remaining,
            };
        }
    }
}

} // namespace

std::size_t ApplyOrderedEvents(
    NetSessionState& session,
    State& state,
    Audio* audio,
    Graphics* graphics
) {
    std::size_t applied_count = 0;
    std::vector<NetEventId> transient_applied_event_ids;
    std::optional<std::uint64_t> pending_snapshot_fingerprint;
    std::stable_sort(
        session.ordered_events.begin(),
        session.ordered_events.end(),
        [](const NetEvent& a, const NetEvent& b) {
            if (a.header.coordinator_order == b.header.coordinator_order) {
                return a.header.event_id < b.header.event_id;
            }
            if (a.header.coordinator_order == 0) {
                return false;
            }
            if (b.header.coordinator_order == 0) {
                return true;
            }
            return a.header.coordinator_order < b.header.coordinator_order;
        }
    );
    for (const NetEvent& event : session.ordered_events) {
        if (session.HasAppliedEvent(event.header.event_id)) {
            continue;
        }
        if (session.role == NetRole::Peer &&
            event.header.coordinator_order > session.next_expected_coordinator_order) {
            continue;
        }
        if (ShouldSkipImmediateLocalApply(session, event)) {
            if (session.MarkEventApplied(event.header.event_id)) {
                NoteAppliedCoordinatorOrder(session, event);
                session.AddEventLog(NetEventLogPhase::SkippedLocalApply, event);
                ++applied_count;
                if (IsTransientStateRepairEvent(event)) {
                    transient_applied_event_ids.push_back(event.header.event_id);
                }
            }
            continue;
        }

        switch (event.type) {
        case NetEventType::EntitySpawned:
            if (const auto* payload = std::get_if<EntitySpawnedEvent>(&event.payload)) {
                ApplyEntitySpawnedEvent(session, state, *payload, graphics);
            }
            break;
        case NetEventType::EntityDamaged:
            if (const auto* payload = std::get_if<EntityDamagedEvent>(&event.payload)) {
                ApplyEntityDamagedEvent(session, state, audio, event.header.source_player_id, *payload);
            }
            break;
        case NetEventType::EntityDeactivated:
            if (const auto* payload = std::get_if<EntityIdEvent>(&event.payload)) {
                ApplyEntityDeactivatedEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::EntityStatePatched:
            if (const auto* payload = std::get_if<EntityStatePatchedEvent>(&event.payload)) {
                ApplyEntityStatePatchedEvent(
                    session,
                    state,
                    graphics,
                    event.header.source_player_id,
                    *payload
                );
            }
            break;
        case NetEventType::EntityHeld:
            if (const auto* payload = std::get_if<EntityHeldEvent>(&event.payload)) {
                ApplyEntityHeldEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::EntityDropped:
            if (const auto* payload = std::get_if<EntityDroppedEvent>(&event.payload)) {
                ApplyEntityDroppedEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::EntityThrown:
            if (const auto* payload = std::get_if<EntityThrownEvent>(&event.payload)) {
                ApplyEntityThrownEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::PlayerStatePatched:
            if (const auto* payload = std::get_if<PlayerStatePatchedEvent>(&event.payload)) {
                ApplyPlayerStatePatchedEvent(session, state, *payload);
            }
            break;
        case NetEventType::RunStatePatched:
            if (const auto* payload = std::get_if<RunStatePatchedEvent>(&event.payload)) {
                state.quest_state.quest_id = payload->quest_id;
                state.frame = payload->frame;
                state.stage_frame = payload->stage_frame;
                state.depth = payload->depth;
                state.points = payload->points;
                state.deaths = payload->deaths;
                state.stage.stage_type = static_cast<StageType>(payload->stage_type);
                state.stage.quest_level_number = payload->quest_level_number;
                state.stage.generation_seed = payload->has_generation_seed != 0
                    ? std::optional<std::uint32_t>(payload->generation_seed)
                    : std::nullopt;
                state.stage.tile_change_generation = payload->tile_change_generation;
                state.stage.gravity = payload->stage_gravity;
                state.stage.border.left.tile = payload->border_left_tile;
                state.stage.border.right.tile = payload->border_right_tile;
                state.stage.border.top.tile = payload->border_top_tile;
                state.stage.border.bottom.tile = payload->border_bottom_tile;
                state.stage.border.wrap_x = payload->border_wrap_x != 0;
                state.stage.border.wrap_y = payload->border_wrap_y != 0;
                state.stage.border.void_death_y = payload->has_void_death_y != 0
                    ? std::optional<int>(payload->void_death_y)
                    : std::nullopt;
                state.stage.camera_clamp_enabled = payload->camera_clamp_enabled != 0;
                state.stage.wrap_transform_active = payload->wrap_transform_active != 0;
                state.game_over = payload->game_over != 0;
                state.win = payload->win != 0;
                state.stage.wrap_padding_tiles = payload->wrap_padding_tiles;
                state.stage.wrap_core_origin_tiles = UVec2::New(
                    payload->wrap_core_origin_x,
                    payload->wrap_core_origin_y
                );
                state.stage.wrap_core_size_tiles = UVec2::New(
                    payload->wrap_core_size_x,
                    payload->wrap_core_size_y
                );
                state.quest_state.classic.made_black_market =
                    payload->classic_made_black_market != 0;
                state.quest_state.classic.made_udjat_eye =
                    payload->classic_made_udjat_eye != 0;
                state.quest_state.classic.has_udjat_eye =
                    payload->classic_has_udjat_eye != 0;
                state.quest_state.classic.made_moai =
                    payload->classic_made_moai != 0;
                state.quest_state.classic.has_hedjet =
                    payload->classic_has_hedjet != 0;
                state.quest_state.classic.has_sceptre =
                    payload->classic_has_sceptre != 0;
                state.quest_state.classic.has_book_of_dead =
                    payload->classic_has_book_of_dead != 0;
                state.sac_altar_favor = payload->sac_altar_favor;
                state.sac_altar_reward_tier = payload->sac_altar_reward_tier;
                if (payload->has_snapshot_fingerprint != 0) {
                    pending_snapshot_fingerprint = payload->snapshot_fingerprint;
                }
            }
            break;
        case NetEventType::TileBroken:
            if (const auto* payload = std::get_if<TileBrokenEvent>(&event.payload)) {
                ApplyTileBrokenEvent(state, audio, *payload);
            }
            break;
        case NetEventType::PresentationCommand:
            if (const auto* payload = std::get_if<PresentationCommandEvent>(&event.payload)) {
                ApplyPresentationCommandEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::TileChanged:
            if (const auto* payload = std::get_if<TileChangedEvent>(&event.payload)) {
                ApplyTileChangedEvent(state, *payload);
            }
            break;
        case NetEventType::FluidCellPatched:
            if (const auto* payload = std::get_if<FluidCellPatchedEvent>(&event.payload)) {
                ApplyFluidCellPatchedEvent(state, *payload);
            }
            break;
        default:
            break;
        }

        if (session.MarkEventApplied(event.header.event_id)) {
            NoteAppliedCoordinatorOrder(session, event);
            session.AddEventLog(NetEventLogPhase::Applied, event);
            ++applied_count;
            if (IsTransientStateRepairEvent(event)) {
                transient_applied_event_ids.push_back(event.header.event_id);
            }
        }
    }
    if (!transient_applied_event_ids.empty()) {
        session.ordered_events.erase(
            std::remove_if(
                session.ordered_events.begin(),
                session.ordered_events.end(),
                [&](const NetEvent& event) {
                    return IsTransientStateRepairEvent(event) &&
                           std::find(
                               transient_applied_event_ids.begin(),
                               transient_applied_event_ids.end(),
                               event.header.event_id
                           ) != transient_applied_event_ids.end();
                }
            ),
            session.ordered_events.end()
        );
        session.applied_event_ids.erase(
            std::remove_if(
                session.applied_event_ids.begin(),
                session.applied_event_ids.end(),
                [&](NetEventId event_id) {
                    return std::find(
                               transient_applied_event_ids.begin(),
                               transient_applied_event_ids.end(),
                               event_id
                           ) != transient_applied_event_ids.end();
                }
            ),
            session.applied_event_ids.end()
        );
    }
    if (pending_snapshot_fingerprint.has_value()) {
        const std::uint64_t actual_fingerprint = ComputeNetworkStateFingerprint(state).value;
        state.net_session.last_snapshot_expected_fingerprint = *pending_snapshot_fingerprint;
        state.net_session.last_snapshot_actual_fingerprint = actual_fingerprint;
        state.net_session.last_snapshot_fingerprint_valid =
            actual_fingerprint == *pending_snapshot_fingerprint;
    }
    return applied_count;
}

} // namespace splonks::network

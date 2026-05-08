#include "world_ops.hpp"

#include "audio.hpp"
#include "buying.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "graphics.hpp"
#include "network/net_gameplay_replication.hpp"
#include "network/net_session.hpp"
#include "stage_break.hpp"
#include "state.hpp"
#include "world_query.hpp"
#include "entities/common/common.hpp"

#include <utility>
#include <vector>

namespace splonks::world_ops {

namespace {

void ApplyAttachmentUseAction(
    State& state,
    const GameplayActionRequested& request,
    AttachmentMode source,
    Graphics& graphics,
    Audio& audio
) {
    if (!request.source_vid.has_value() || !request.target_vid.has_value()) {
        return;
    }

    Entity* const holder = state.entity_manager.GetEntityMut(*request.source_vid);
    Entity* const item = state.entity_manager.GetEntityMut(*request.target_vid);
    if (holder == nullptr || item == nullptr || !holder->active || !item->active) {
        return;
    }

    const bool is_attached_item =
        source == AttachmentMode::Held
            ? holder->holding_vid.has_value() && *holder->holding_vid == item->vid
            : holder->back_vid.has_value() && *holder->back_vid == item->vid;
    if (!is_attached_item || item->held_by_vid != holder->vid || item->attachment_mode != source) {
        return;
    }

    if (PlayerSlot* const slot = state.players.FindByEntityVid(holder->vid);
        slot != nullptr && slot->connection_kind == PlayerConnectionKind::Remote) {
        slot->inputs.left.down = request.direction.x < 0;
        slot->inputs.right.down = request.direction.x > 0;
        slot->inputs.up.down = request.direction.y < 0;
        slot->inputs.down.down = request.direction.y > 0;
        if (source == AttachmentMode::Held) {
            slot->inputs.attack.down = request.param_a != 0;
        } else {
            slot->inputs.use_button.down = request.param_a != 0;
        }
        slot->immediate_inputs = slot->inputs;
    }

    if (request.param_a != 0) {
        UseEntity(*item, holder->vid, source);
    } else {
        StopUsingEntity(*item);
    }
    if (item->on_use != nullptr) {
        item->on_use(item->vid.id, state, graphics, audio);
    }
    if (holder->active) {
        PatchEntityState(state, *holder, *holder);
    }
    if (item->active) {
        PatchEntityState(state, *item, *item);
    }
}

bool AreEntitiesOverlappingForInteract(
    const Entity& source,
    const Entity& target,
    const State& state,
    const Graphics& graphics
) {
    const AABB source_aabb = entities::common::GetContactAabbForEntity(source, graphics);
    const Vec2 source_center = (source_aabb.tl + source_aabb.br) / 2.0F;
    const AABB target_aabb = GetNearestWorldAabb(
        state.stage,
        source_center,
        entities::common::GetContactAabbForEntity(target, graphics)
    );
    return AabbsIntersect(source_aabb, target_aabb);
}

bool TryApplyInteractEntityAction(
    State& state,
    const GameplayActionRequested& request,
    Graphics& graphics,
    Audio& audio
) {
    if (!request.source_vid.has_value() || !request.target_vid.has_value()) {
        return false;
    }

    Entity* const source = state.entity_manager.GetEntityMut(*request.source_vid);
    Entity* const target = state.entity_manager.GetEntityMut(*request.target_vid);
    if (source == nullptr || target == nullptr ||
        !source->active || !target->active ||
        source->condition == EntityCondition::Dead) {
        return false;
    }

    if (!AreEntitiesOverlappingForInteract(*source, *target, state, graphics)) {
        return false;
    }

    if (target->buyable.active) {
        return TryBuyEntity(target->vid.id, source->vid.id, state, graphics, audio);
    }

    const EntityArchetype& archetype = GetEntityArchetype(target->type_);
    if (archetype.on_interact == nullptr) {
        return false;
    }
    return archetype.on_interact(target->vid.id, source->vid.id, state, graphics, audio);
}

bool TryApplyCollectEntityAction(
    State& state,
    const GameplayActionRequested& request,
    Graphics& graphics,
    Audio& audio
) {
    if (!request.source_vid.has_value() || !request.target_vid.has_value()) {
        return false;
    }

    Entity* const source = state.entity_manager.GetEntityMut(*request.source_vid);
    Entity* const target = state.entity_manager.GetEntityMut(*request.target_vid);
    if (source == nullptr || target == nullptr ||
        !source->active || !target->active ||
        source->condition == EntityCondition::Dead ||
        target->buyable.active ||
        !source->can_collect_pickups ||
        !AreEntitiesOverlappingForInteract(*source, *target, state, graphics)) {
        return false;
    }

    const EntityArchetype& archetype = GetEntityArchetype(target->type_);
    if (archetype.on_entity_contact == nullptr) {
        return false;
    }

    const bool was_active = target->active;
    (void)archetype.on_entity_contact(
        target->vid.id,
        source->vid.id,
        entities::common::ContactContext{
            .phase = entities::common::ContactPhase::SweptEntered,
            .has_impact = false,
            .mover_vid = source->vid,
            .other_vid = target->vid,
        },
        state,
        &graphics,
        &audio
    );
    return was_active && !target->active;
}

} // namespace

void RequestGameplayAction(State& state, const GameplayActionRequested& action) {
    if (state.net_session.role == network::NetRole::Peer) {
        network::ReplicateActionRequest(state, action);
        return;
    }
    state.pending_gameplay_actions.push_back(action);
}

void QueuePendingGameplayAction(State& state, const GameplayActionRequested& action) {
    state.pending_gameplay_actions.push_back(action);
}

bool TryRequestOrApplyInteractEntity(
    VID source_vid,
    VID target_vid,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    GameplayActionRequested request{
        .kind = GameplayActionKind::InteractEntity,
        .source_vid = source_vid,
        .target_vid = target_vid,
    };
    if (state.net_session.role == network::NetRole::Peer) {
        RequestGameplayAction(state, request);
        return true;
    }
    return TryApplyInteractEntityAction(state, request, graphics, audio);
}

void ProcessPendingGameplayActions(State& state, Graphics& graphics, Audio& audio) {
    std::vector<GameplayActionRequested> actions = std::move(state.pending_gameplay_actions);
    state.pending_gameplay_actions.clear();

    for (const GameplayActionRequested& action : actions) {
        if (state.net_session.role == network::NetRole::Peer) {
            network::ReplicateActionRequest(state, action);
            continue;
        }

        switch (action.kind) {
        case GameplayActionKind::UseTool:
            if (action.source_vid.has_value()) {
                (void)entities::common::TryUseToolSlot(
                    action.source_vid->id,
                    state,
                    graphics,
                    audio,
                    action.param_a,
                    true,
                    nullptr,
                    action.velocity
                );
            }
            break;
        case GameplayActionKind::PickupEntity:
            if (action.source_vid.has_value() && action.target_vid.has_value()) {
                (void)entities::common::TryPickupEntityByVid(
                    *action.source_vid,
                    *action.target_vid,
                    state,
                    graphics
                );
            }
            break;
        case GameplayActionKind::DropEntity:
            if (action.source_vid.has_value() && action.target_vid.has_value()) {
                (void)entities::common::TryDropEntityByVid(
                    *action.source_vid,
                    *action.target_vid,
                    state,
                    graphics
                );
            }
            break;
        case GameplayActionKind::ThrowEntity:
            if (action.source_vid.has_value() && action.target_vid.has_value()) {
                (void)entities::common::TryThrowEntityByVid(
                    *action.source_vid,
                    *action.target_vid,
                    action.velocity,
                    state,
                    graphics,
                    audio
                );
            }
            break;
        case GameplayActionKind::UseHeldEntity:
            ApplyAttachmentUseAction(state, action, AttachmentMode::Held, graphics, audio);
            break;
        case GameplayActionKind::UseBackEntity:
            ApplyAttachmentUseAction(state, action, AttachmentMode::Back, graphics, audio);
            break;
        case GameplayActionKind::PutHeldEntityOnBack:
            if (action.source_vid.has_value() && action.target_vid.has_value()) {
                (void)entities::common::TryPutHeldEntityOnBackByVid(
                    *action.source_vid,
                    *action.target_vid,
                    state,
                    graphics
                );
            }
            break;
        case GameplayActionKind::TakeOffBackEntity:
            if (action.source_vid.has_value() && action.target_vid.has_value()) {
                (void)entities::common::TryTakeOffBackEntityByVid(
                    *action.source_vid,
                    *action.target_vid,
                    state,
                    graphics
                );
            }
            break;
        case GameplayActionKind::InteractEntity:
            (void)TryApplyInteractEntityAction(state, action, graphics, audio);
            break;
        case GameplayActionKind::CollectEntity:
            (void)TryApplyCollectEntityAction(state, action, graphics, audio);
            break;
        case GameplayActionKind::PushEntity:
            if (action.source_vid.has_value() && action.target_vid.has_value()) {
                (void)entities::common::TryApplyPushEntityAction(
                    *action.source_vid,
                    *action.target_vid,
                    action.velocity.x,
                    state,
                    graphics
                );
            }
            break;
        case GameplayActionKind::BreakTile:
            BreakStageTilesAtCoords({action.tile_pos}, state, audio);
            break;
        case GameplayActionKind::DamageEntity:
            if (action.target_vid.has_value()) {
                (void)entities::common::TryDamageEntity(
                    action.target_vid->id,
                    state,
                    audio,
                    action.damage_type,
                    action.amount,
                    entities::common::DamageOptions{
                        .source_vid = action.source_vid,
                        .allow_remote_player_target = true,
                    }
                );
            }
            break;
        case GameplayActionKind::HitEntity:
            if (action.target_vid.has_value()) {
                (void)entities::common::TryHitEntity(
                    action.target_vid->id,
                    state,
                    audio,
                    action.damage_type,
                    action.amount,
                    entities::common::HitOptions{
                        .source_vid = action.source_vid,
                        .knockback = entities::common::KnockbackSpec{
                            .velocity = action.velocity,
                            .clear_velocity = action.clear_velocity,
                            .clear_acceleration = action.clear_acceleration,
                            .thrown_by = action.source_vid,
                            .thrown_immunity_timer = action.thrown_immunity_timer,
                            .projectile_contact_damage_type =
                                action.projectile_contact_damage_type,
                            .projectile_contact_damage_amount =
                                action.projectile_contact_damage_amount,
                            .projectile_contact_duration = action.projectile_contact_duration,
                        },
                        .allow_remote_player_target = true,
                        .knockback_on_no_damage = action.knockback_on_no_damage,
                    }
                );
            }
            break;
        case GameplayActionKind::None:
            break;
        }
    }
}

} // namespace splonks::world_ops

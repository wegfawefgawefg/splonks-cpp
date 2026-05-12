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
    VID source_vid,
    VID target_vid,
    IVec2 direction,
    GameplayUseEdge use_edge,
    AttachmentMode source,
    Graphics& graphics,
    Audio& audio
) {
    Entity* const holder = state.entity_manager.GetEntityMut(source_vid);
    Entity* const item = state.entity_manager.GetEntityMut(target_vid);
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
        const bool use_down = use_edge == GameplayUseEdge::Press;
        slot->inputs.left.down = direction.x < 0;
        slot->inputs.right.down = direction.x > 0;
        slot->inputs.up.down = direction.y < 0;
        slot->inputs.down.down = direction.y > 0;
        if (source == AttachmentMode::Held) {
            slot->inputs.attack.down = use_down;
        } else {
            slot->inputs.use_button.down = use_down;
        }
        slot->immediate_inputs = slot->inputs;
    }
    if (direction.x < 0) {
        holder->facing = LeftOrRight::Left;
        item->facing = LeftOrRight::Left;
    } else if (direction.x > 0) {
        holder->facing = LeftOrRight::Right;
        item->facing = LeftOrRight::Right;
    }

    if (use_edge == GameplayUseEdge::Press) {
        PressUseEntity(*item, holder->vid, source);
    } else if (use_edge == GameplayUseEdge::Release) {
        ReleaseUseEntity(*item, holder->vid, source);
    } else {
        return;
    }
    if (item->on_use != nullptr) {
        item->on_use(item->vid.id, state, graphics, audio);
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
    const InteractEntityAction& action,
    Graphics& graphics,
    Audio& audio
) {
    Entity* const source = state.entity_manager.GetEntityMut(action.source_vid);
    Entity* const target = state.entity_manager.GetEntityMut(action.target_vid);
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
    const CollectEntityAction& action,
    Graphics& graphics,
    Audio& audio
) {
    Entity* const source = state.entity_manager.GetEntityMut(action.source_vid);
    Entity* const target = state.entity_manager.GetEntityMut(action.target_vid);
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

void ApplyGameplayAction(State& state, const GameplayActionRequested& action, Graphics& graphics, Audio& audio) {
    std::visit(
        GameplayActionVisitor{
            [](const std::monostate&) {},
            [&](const UseToolAction& payload) {
                (void)entities::common::TryUseToolSlot(
                    payload.source_vid.id,
                    state,
                    graphics,
                    audio,
                    payload.tool_slot,
                    true,
                    nullptr,
                    payload.velocity
                );
            },
            [&](const PickupEntityAction& payload) {
                (void)entities::common::TryPickupEntityByVid(
                    payload.source_vid,
                    payload.target_vid,
                    state,
                    graphics
                );
            },
            [&](const DropEntityAction& payload) {
                (void)entities::common::TryDropEntityByVid(
                    payload.source_vid,
                    payload.target_vid,
                    state,
                    graphics
                );
            },
            [&](const ThrowEntityAction& payload) {
                (void)entities::common::TryThrowEntityByVid(
                    payload.source_vid,
                    payload.target_vid,
                    payload.velocity,
                    state,
                    graphics,
                    audio
                );
            },
            [&](const UseHeldEntityAction& payload) {
                ApplyAttachmentUseAction(
                    state,
                    payload.source_vid,
                    payload.target_vid,
                    payload.direction,
                    payload.use_edge,
                    AttachmentMode::Held,
                    graphics,
                    audio
                );
            },
            [&](const UseBackEntityAction& payload) {
                ApplyAttachmentUseAction(
                    state,
                    payload.source_vid,
                    payload.target_vid,
                    payload.direction,
                    payload.use_edge,
                    AttachmentMode::Back,
                    graphics,
                    audio
                );
            },
            [&](const PutHeldEntityOnBackAction& payload) {
                (void)entities::common::TryPutHeldEntityOnBackByVid(
                    payload.source_vid,
                    payload.target_vid,
                    state,
                    graphics
                );
            },
            [&](const TakeOffBackEntityAction& payload) {
                (void)entities::common::TryTakeOffBackEntityByVid(
                    payload.source_vid,
                    payload.target_vid,
                    state,
                    graphics
                );
            },
            [&](const InteractEntityAction& payload) {
                (void)TryApplyInteractEntityAction(state, payload, graphics, audio);
            },
            [&](const CollectEntityAction& payload) {
                (void)TryApplyCollectEntityAction(state, payload, graphics, audio);
            },
            [&](const PushEntityAction& payload) {
                (void)entities::common::TryApplyPushEntityAction(
                    payload.source_vid,
                    payload.target_vid,
                    payload.velocity.x,
                    state,
                    graphics
                );
            },
            [&](const BreakTileAction& payload) {
                BreakStageTilesAtCoords({payload.tile_pos}, state, audio);
            },
            [&](const DamageEntityAction& payload) {
                const Entity* const target = state.entity_manager.GetEntity(payload.target_vid);
                if (payload.damage_type == DamageType::Fall &&
                    (target == nullptr || target->condition != EntityCondition::Normal)) {
                    return;
                }
                (void)entities::common::TryDamageEntity(
                    payload.target_vid.id,
                    state,
                    audio,
                    payload.damage_type,
                    payload.amount,
                    entities::common::DamageOptions{
                        .source_vid = payload.source_vid,
                        .allow_remote_player_target = true,
                    }
                );
            },
            [&](const HitEntityAction& payload) {
                (void)entities::common::TryHitEntity(
                    payload.target_vid.id,
                    state,
                    audio,
                    payload.damage_type,
                    payload.amount,
                    entities::common::HitOptions{
                        .source_vid = payload.source_vid,
                        .knockback = entities::common::KnockbackSpec{
                            .velocity = payload.velocity,
                            .clear_velocity = payload.clear_velocity,
                            .clear_acceleration = payload.clear_acceleration,
                            .thrown_by = payload.source_vid,
                            .thrown_immunity_timer = payload.thrown_immunity_timer,
                            .projectile_contact_damage_type =
                                payload.projectile_contact_damage_type,
                            .projectile_contact_damage_amount =
                                payload.projectile_contact_damage_amount,
                            .projectile_contact_duration = payload.projectile_contact_duration,
                        },
                        .allow_remote_player_target = true,
                        .knockback_on_no_damage = payload.knockback_on_no_damage,
                    }
                );
            },
        },
        action.payload
    );
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
    GameplayActionRequested request = InteractEntityAction{
        .source_vid = source_vid,
        .target_vid = target_vid,
    };
    if (state.net_session.role == network::NetRole::Peer) {
        RequestGameplayAction(state, request);
        return true;
    }
    return TryApplyInteractEntityAction(
        state,
        InteractEntityAction{
            .source_vid = source_vid,
            .target_vid = target_vid,
        },
        graphics,
        audio
    );
}

void ProcessPendingGameplayActions(State& state, Graphics& graphics, Audio& audio) {
    std::vector<GameplayActionRequested> actions = std::move(state.pending_gameplay_actions);
    state.pending_gameplay_actions.clear();

    for (const GameplayActionRequested& action : actions) {
        if (state.net_session.role == network::NetRole::Peer) {
            network::ReplicateActionRequest(state, action);
            continue;
        }

        ApplyGameplayAction(state, action, graphics, audio);
    }
}

} // namespace splonks::world_ops

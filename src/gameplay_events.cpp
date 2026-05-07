#include "gameplay_events.hpp"

#include "audio.hpp"
#include "buying.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entity/display_states.hpp"
#include "entity/replicated_runtime_flags.hpp"
#include "graphics.hpp"
#include "network/net_gameplay_replication.hpp"
#include "network/net_progression.hpp"
#include "network/net_session.hpp"
#include "stage_break.hpp"
#include "state.hpp"
#include "world_query.hpp"
#include "entities/common/common.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace splonks {

namespace {

struct AnimationPresentationSnapshot {
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

AnimationPresentationSnapshot CaptureCurrentPresentation(const Entity& entity) {
    return AnimationPresentationSnapshot{
        .animate = static_cast<std::uint8_t>(entity.frame_data_animator.animate ? 1 : 0),
        .animation_id = entity.frame_data_animator.animation_id,
        .animation_frame = static_cast<std::uint16_t>(std::min<std::size_t>(
            entity.frame_data_animator.current_frame,
            std::numeric_limits<std::uint16_t>::max()
        )),
        .animation_time = entity.frame_data_animator.current_time,
        .animation_speed = entity.frame_data_animator.speed,
    };
}

AnimationPresentationSnapshot CaptureDamagePresentation(const Entity& entity) {
    AnimationPresentationSnapshot presentation = CaptureCurrentPresentation(entity);

    std::optional<EntityDisplayState> canonical_display_state;
    if (entity.health == 0 || entity.condition == EntityCondition::Dead) {
        canonical_display_state = EntityDisplayState::Dead;
    } else if (entity.condition == EntityCondition::Stunned) {
        canonical_display_state = EntityDisplayState::Stunned;
    }

    if (canonical_display_state.has_value()) {
        const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
            .type_ = entity.type_,
            .display_state = *canonical_display_state,
        });
        if (selection.has_value()) {
            presentation.animate = static_cast<std::uint8_t>(selection->animate ? 1 : 0);
            presentation.animation_id = selection->animation_id;
            presentation.animation_frame = static_cast<std::uint16_t>(
                selection->has_forced_frame
                    ? std::min<std::size_t>(
                          selection->forced_frame,
                          std::numeric_limits<std::uint16_t>::max()
                      )
                    : 0
            );
            presentation.animation_time = 0.0F;
            presentation.animation_speed = 1.0F;
        }
    }

    return presentation;
}

void ApplyAttachmentUseAction(
    State& state,
    const GameplayActionRequested& request,
    AttachmentMode source
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

void EmitStageExitRequested(State& state, StageExitId exit_id, PlayerId player_id) {
    GameplayEvent event;
    event.type = GameplayEventType::StageExitRequested;
    event.stage_exit = GameplayStageExitRequested{
        .exit_id = exit_id,
        .player_id = player_id,
    };
    state.gameplay_events.push_back(event);
}

void EmitStageTransitionRequested(State& state, const StageTransitionTarget& target, PlayerId player_id) {
    GameplayEvent event;
    event.type = GameplayEventType::StageTransitionRequested;
    event.stage_transition = GameplayStageTransitionRequested{
        .target = target,
        .player_id = player_id,
    };
    state.gameplay_events.push_back(event);
}

void EmitGameplayActionRequested(State& state, const GameplayActionRequested& request) {
    GameplayEvent event;
    event.type = GameplayEventType::ActionRequested;
    event.action_requested = request;
    state.gameplay_events.push_back(event);
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
        EmitGameplayActionRequested(state, request);
        return true;
    }
    return TryApplyInteractEntityAction(state, request, graphics, audio);
}

void EmitEntitySpawnedGameplayEvent(
    State& state,
    const Entity& spawned_entity,
    std::optional<VID> held_by_vid
) {
    GameplayEvent event;
    event.type = GameplayEventType::EntitySpawned;
    const AnimationPresentationSnapshot presentation = CaptureCurrentPresentation(spawned_entity);
    event.entity_spawned = GameplayEntitySpawned{
        .entity_vid = spawned_entity.vid,
        .held_by_vid = held_by_vid,
        .entity_type = spawned_entity.type_,
        .pos = spawned_entity.pos,
        .vel = spawned_entity.vel,
        .acc = spawned_entity.acc,
        .counter_a = spawned_entity.counter_a,
        .counter_b = spawned_entity.counter_b,
        .use_pressed = spawned_entity.use_state.pressed,
        .animate = presentation.animate,
        .animation_id = presentation.animation_id,
        .animation_frame = presentation.animation_frame,
        .animation_time = presentation.animation_time,
        .animation_speed = presentation.animation_speed,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityDeactivatedGameplayEvent(State& state, const Entity& entity) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityDeactivated;
    event.entity_deactivated = GameplayEntityDeactivated{
        .entity_vid = entity.vid,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityHeldGameplayEvent(
    State& state,
    const Entity& holder,
    const Entity& held,
    AttachmentMode attachment_mode
) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityHeld;
    event.entity_held = GameplayEntityHeld{
        .holder_vid = holder.vid,
        .held_vid = held.vid,
        .attachment_mode = attachment_mode,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityDroppedGameplayEvent(
    State& state,
    const Entity& entity,
    std::optional<VID> dropped_by_vid
) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityDropped;
    event.entity_dropped = GameplayEntityDropped{
        .entity_vid = entity.vid,
        .dropped_by_vid = dropped_by_vid,
        .pos = entity.pos,
        .vel = entity.vel,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityThrownGameplayEvent(
    State& state,
    const Entity& thrower,
    const Entity& thrown,
    Vec2 throw_velocity
) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityThrown;
    event.entity_thrown = GameplayEntityThrown{
        .thrower_vid = thrower.vid,
        .entity_vid = thrown.vid,
        .pos = thrown.pos,
        .vel = throw_velocity,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityDamagedGameplayEvent(
    State& state,
    const Entity& entity,
    DamageType damage_type,
    unsigned int amount,
    std::optional<VID> source_vid
) {
    const AnimationPresentationSnapshot presentation = CaptureDamagePresentation(entity);
    GameplayEvent event;
    event.type = GameplayEventType::EntityDamaged;
    event.entity_damaged = GameplayEntityDamaged{
        .entity_vid = entity.vid,
        .source_vid = source_vid,
        .damage_type = damage_type,
        .amount = amount,
        .remaining_health = entity.health,
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
        .stun_timer = entity.stun_timer,
        .projectile_contact_timer = entity.projectile_contact_timer,
        .condition = static_cast<std::uint8_t>(entity.condition),
        .grounded = static_cast<std::uint8_t>(entity.grounded ? 1 : 0),
        .animate = presentation.animate,
        .animation_id = presentation.animation_id,
        .animation_frame = presentation.animation_frame,
        .animation_time = presentation.animation_time,
        .animation_speed = presentation.animation_speed,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityStatePatchedGameplayEvent(State& state, const Entity& source, const Entity& entity) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityStatePatched;
    const AnimationPresentationSnapshot presentation = CaptureCurrentPresentation(entity);
    event.entity_state_patched = GameplayEntityStatePatched{
        .entity_vid = entity.vid,
        .source_vid = source.vid,
        .entity_a_vid = entity.entity_a,
        .entity_b_vid = entity.entity_b,
        .entity_c_vid = entity.entity_c,
        .entity_d_vid = entity.entity_d,
        .holding_vid = entity.holding_vid,
        .held_by_vid = entity.held_by_vid,
        .back_vid = entity.back_vid,
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
        .counter_a = entity.counter_a,
        .counter_b = entity.counter_b,
        .counter_c = entity.counter_c,
        .counter_d = entity.counter_d,
        .threshold_a = entity.threshold_a,
        .threshold_b = entity.threshold_b,
        .point_a = entity.point_a,
        .point_b = entity.point_b,
        .point_c = entity.point_c,
        .point_d = entity.point_d,
        .health = entity.health,
        .stun_timer = entity.stun_timer,
        .projectile_contact_timer = entity.projectile_contact_timer,
        .rotation = entity.rotation,
        .condition = static_cast<std::uint8_t>(entity.condition),
        .grounded = static_cast<std::uint8_t>(entity.grounded ? 1 : 0),
        .active = static_cast<std::uint8_t>(entity.active ? 1 : 0),
        .has_physics = static_cast<std::uint8_t>(entity.has_physics ? 1 : 0),
        .can_collide = static_cast<std::uint8_t>(entity.can_collide ? 1 : 0),
        .can_apply_projectile_contact =
            static_cast<std::uint8_t>(entity.can_apply_projectile_contact ? 1 : 0),
        .facing = static_cast<std::uint8_t>(entity.facing == LeftOrRight::Right ? 1 : 0),
        .ai_state = static_cast<std::uint8_t>(entity.ai_state),
        .wanted = static_cast<std::uint8_t>(entity.wanted ? 1 : 0),
        .attachment_mode = static_cast<std::uint8_t>(entity.attachment_mode),
        .draw_layer = static_cast<std::uint8_t>(entity.draw_layer),
        .runtime_flags = CaptureReplicatedRuntimeFlags(entity),
        .buyable_active = static_cast<std::uint8_t>(entity.buyable.active ? 1 : 0),
        .buyable_display_quantity = entity.buyable.display_quantity,
        .buyable_display_icon_animation_id =
            entity.buyable.display_icon_animation_id.value_or(kInvalidFrameDataId),
        .buyable_shop_owner_vid = entity.buyable.shop_owner_vid,
        .animate = presentation.animate,
        .animation_id = presentation.animation_id,
        .animation_frame = presentation.animation_frame,
        .animation_time = presentation.animation_time,
        .animation_speed = presentation.animation_speed,
    };
    state.gameplay_events.push_back(event);
}

void EmitPlayerStatePatchedGameplayEvent(State& state, const Entity& player) {
    GameplayEvent event;
    event.type = GameplayEventType::PlayerStatePatched;
    event.player_state_patched = GameplayPlayerStatePatched{
        .player_vid = player.vid,
    };
    state.gameplay_events.push_back(event);
}

void EmitRunStatePatchedGameplayEvent(State& state) {
    GameplayEvent event;
    event.type = GameplayEventType::RunStatePatched;
    state.gameplay_events.push_back(event);
}

void EmitTileChangedGameplayEvent(
    State& state,
    const IVec2& tile_pos,
    Tile tile,
    TileRotation rotation,
    GameplayTileLayer layer
) {
    GameplayEvent event;
    event.type = GameplayEventType::TileChanged;
    event.tile_changed = GameplayTileChanged{
        .tile_pos = tile_pos,
        .tile = tile,
        .rotation = NormalizeTileRotation(rotation),
        .layer = layer,
    };
    state.gameplay_events.push_back(event);
}

void EmitTileBrokenGameplayEvent(State& state, const IVec2& tile_pos) {
    GameplayEvent event;
    event.type = GameplayEventType::TileBroken;
    event.tile_broken = GameplayTileBroken{
        .tile_pos = tile_pos,
    };
    state.gameplay_events.push_back(event);
}

void EmitRopeTilePlacedGameplayEvent(State& state, const Entity& source_entity, const IVec2& tile_pos) {
    GameplayEvent event;
    event.type = GameplayEventType::RopeTilePlaced;
    event.rope_tile_placed = GameplayRopeTilePlaced{
        .source_vid = source_entity.vid,
        .tile_pos = tile_pos,
    };
    state.gameplay_events.push_back(event);
}

void EmitPresentationCommandGameplayEvent(State& state, const PresentationCommand& command) {
    GameplayEvent event;
    event.type = GameplayEventType::PresentationCommand;
    event.presentation_command = command;
    state.gameplay_events.push_back(event);
}

void ProcessGameplayEvents(State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;

    const std::vector<GameplayEvent> events = std::move(state.gameplay_events);
    state.gameplay_events.clear();

    for (const GameplayEvent& event : events) {
        switch (event.type) {
        case GameplayEventType::StageExitRequested:
            if (state.pending_stage_transition.has_value()) {
                continue;
            }
            if (!IsStageExitAllowed(state, event.stage_exit.exit_id)) {
                continue;
            }
            if (state.net_session.role == network::NetRole::Peer) {
                continue;
            }
            QueueStageExitTransition(state, event.stage_exit.exit_id);
            break;
        case GameplayEventType::StageTransitionRequested:
            if (state.pending_stage_transition.has_value() ||
                state.net_session.role == network::NetRole::Peer) {
                continue;
            }
            QueueStageTransition(state, event.stage_transition.target);
            break;
        case GameplayEventType::ActionRequested:
            if (state.net_session.role == network::NetRole::Peer) {
                network::ReplicateGameplayEvent(state, event);
                continue;
            }
            switch (event.action_requested.kind) {
            case GameplayActionKind::UseTool:
                if (event.action_requested.source_vid.has_value()) {
                    (void)entities::common::TryUseToolSlot(
                        event.action_requested.source_vid->id,
                        state,
                        graphics,
                        audio,
                        event.action_requested.param_a,
                        true,
                        nullptr,
                        event.action_requested.velocity
                    );
                }
                break;
            case GameplayActionKind::PickupEntity:
                if (event.action_requested.source_vid.has_value() &&
                    event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryPickupEntityByVid(
                        *event.action_requested.source_vid,
                        *event.action_requested.target_vid,
                        state,
                        graphics
                    );
                }
                break;
            case GameplayActionKind::DropEntity:
                if (event.action_requested.source_vid.has_value() &&
                    event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryDropEntityByVid(
                        *event.action_requested.source_vid,
                        *event.action_requested.target_vid,
                        state,
                        graphics
                    );
                }
                break;
            case GameplayActionKind::ThrowEntity:
                if (event.action_requested.source_vid.has_value() &&
                    event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryThrowEntityByVid(
                        *event.action_requested.source_vid,
                        *event.action_requested.target_vid,
                        event.action_requested.velocity,
                        state,
                        graphics,
                        audio
                    );
                }
                break;
            case GameplayActionKind::UseHeldEntity:
                ApplyAttachmentUseAction(state, event.action_requested, AttachmentMode::Held);
                break;
            case GameplayActionKind::UseBackEntity:
                ApplyAttachmentUseAction(state, event.action_requested, AttachmentMode::Back);
                break;
            case GameplayActionKind::PutHeldEntityOnBack:
                if (event.action_requested.source_vid.has_value() &&
                    event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryPutHeldEntityOnBackByVid(
                        *event.action_requested.source_vid,
                        *event.action_requested.target_vid,
                        state,
                        graphics
                    );
                }
                break;
            case GameplayActionKind::TakeOffBackEntity:
                if (event.action_requested.source_vid.has_value() &&
                    event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryTakeOffBackEntityByVid(
                        *event.action_requested.source_vid,
                        *event.action_requested.target_vid,
                        state,
                        graphics
                    );
                }
                break;
            case GameplayActionKind::InteractEntity:
                (void)TryApplyInteractEntityAction(state, event.action_requested, graphics, audio);
                break;
            case GameplayActionKind::CollectEntity:
                (void)TryApplyCollectEntityAction(state, event.action_requested, graphics, audio);
                break;
            case GameplayActionKind::PushEntity:
                if (event.action_requested.source_vid.has_value() &&
                    event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryApplyPushEntityAction(
                        *event.action_requested.source_vid,
                        *event.action_requested.target_vid,
                        event.action_requested.velocity.x,
                        state,
                        graphics
                    );
                }
                break;
            case GameplayActionKind::BreakTile:
                BreakStageTilesAtCoords({event.action_requested.tile_pos}, state, audio);
                break;
            case GameplayActionKind::DamageEntity:
                if (event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryDamageEntity(
                        event.action_requested.target_vid->id,
                        state,
                        audio,
                        event.action_requested.damage_type,
                        event.action_requested.amount,
                        entities::common::DamageOptions{
                            .source_vid = event.action_requested.source_vid,
                            .allow_remote_player_target = true,
                        }
                    );
                }
                break;
            case GameplayActionKind::HitEntity:
                if (event.action_requested.target_vid.has_value()) {
                    (void)entities::common::TryHitEntity(
                        event.action_requested.target_vid->id,
                        state,
                        audio,
                        event.action_requested.damage_type,
                        event.action_requested.amount,
                        entities::common::HitOptions{
                            .source_vid = event.action_requested.source_vid,
                            .knockback = entities::common::KnockbackSpec{
                                .velocity = event.action_requested.velocity,
                                .clear_velocity = event.action_requested.clear_velocity,
                                .clear_acceleration = event.action_requested.clear_acceleration,
                                .thrown_by = event.action_requested.source_vid,
                                .thrown_immunity_timer =
                                    event.action_requested.thrown_immunity_timer,
                                .projectile_contact_damage_type =
                                    event.action_requested.projectile_contact_damage_type,
                                .projectile_contact_damage_amount =
                                    event.action_requested.projectile_contact_damage_amount,
                                .projectile_contact_duration =
                                    event.action_requested.projectile_contact_duration,
                            },
                        }
                    );
                }
                break;
            case GameplayActionKind::None:
                break;
            }
            break;
        case GameplayEventType::EntitySpawned:
        case GameplayEventType::EntityDeactivated:
        case GameplayEventType::EntityHeld:
        case GameplayEventType::EntityDropped:
        case GameplayEventType::EntityThrown:
        case GameplayEventType::EntityDamaged:
        case GameplayEventType::EntityStatePatched:
        case GameplayEventType::PlayerStatePatched:
        case GameplayEventType::RunStatePatched:
        case GameplayEventType::TileChanged:
        case GameplayEventType::TileBroken:
        case GameplayEventType::RopeTilePlaced:
        case GameplayEventType::PresentationCommand:
            network::ReplicateGameplayEvent(state, event);
            break;
        }
    }
}

} // namespace splonks

#include "world_ops.hpp"

#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entity/display_states.hpp"
#include "entity/replicated_runtime_flags.hpp"
#include "entities/common/common.hpp"
#include "network/net_gameplay_replication.hpp"
#include "state.hpp"

#include <algorithm>
#include <limits>

namespace splonks::world_ops {

namespace {

struct AnimationPresentationSnapshot {
    std::uint8_t animate = 0;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

AnimationPresentationSnapshot CaptureCurrentPresentation(const Entity& entity) {
    return AnimationPresentationSnapshot{
        .animate = static_cast<std::uint8_t>(entity.frame_data_animator.animate ? 1 : 0),
        .animation_loop = static_cast<std::uint8_t>(entity.frame_data_animator.loop ? 1 : 0),
        .animation_finished = static_cast<std::uint8_t>(entity.frame_data_animator.finished ? 1 : 0),
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
            presentation.animation_loop = 1;
            presentation.animation_finished = 0;
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

} // namespace

Entity* SpawnConfiguredEntity(
    State& state,
    const EntitySpawnSetup& setup,
    std::optional<VID> held_by_vid
) {
    if (state.net_session.role == network::NetRole::Peer) {
        return nullptr;
    }

    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return nullptr;
    }

    if (setup) {
        setup(*entity);
    }
    const AnimationPresentationSnapshot presentation = CaptureCurrentPresentation(*entity);
    network::ReplicateEntitySpawned(
        state,
        GameplayEntitySpawned{
            .entity_vid = entity->vid,
            .held_by_vid = held_by_vid,
            .entity_type = entity->type_,
            .pos = entity->pos,
            .vel = entity->vel,
            .acc = entity->acc,
            .size = entity->size,
            .counter_a = entity->counter_a,
            .counter_b = entity->counter_b,
            .movement_flags = entity->movement_flags,
            .use_pressed = entity->use_state.pressed,
            .animate = presentation.animate,
            .animation_loop = presentation.animation_loop,
            .animation_finished = presentation.animation_finished,
            .animation_id = presentation.animation_id,
            .animation_frame = presentation.animation_frame,
            .animation_time = presentation.animation_time,
            .animation_speed = presentation.animation_speed,
        }
    );
    return entity;
}

Entity* SpawnEntity(
    State& state,
    EntityType type_,
    const EntitySpawnSetup& setup,
    std::optional<VID> held_by_vid
) {
    return SpawnConfiguredEntity(
        state,
        [&](Entity& entity) {
            SetEntityAs(entity, type_);
            if (setup) {
                setup(entity);
            }
        },
        held_by_vid
    );
}

bool DeactivateEntity(State& state, VID entity_vid) {
    if (state.net_session.role == network::NetRole::Peer) {
        return false;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(entity_vid);
    if (entity == nullptr || !entity->active) {
        return false;
    }

    entities::common::ReleaseEntityFromHolder(*entity, state);

    network::ReplicateEntityDeactivated(
        state,
        GameplayEntityDeactivated{
            .entity_vid = entity->vid,
        }
    );
    state.entity_manager.SetInactive(entity->vid.id);
    return true;
}

void PatchEntityState(State& state, const Entity& source, const Entity& entity) {
    const AnimationPresentationSnapshot presentation = CaptureCurrentPresentation(entity);
    network::ReplicateEntityStatePatched(
        state,
        GameplayEntityStatePatched{
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
            .size = entity.size,
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
            .coyote_time = entity.coyote_time,
            .fall_timer = entity.fall_timer,
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
            .movement_flags = entity.movement_flags,
            .runtime_flags = CaptureReplicatedRuntimeFlags(entity),
            .buyable_active = static_cast<std::uint8_t>(entity.buyable.active ? 1 : 0),
            .buyable_display_quantity = entity.buyable.display_quantity,
            .buyable_display_icon_animation_id =
                entity.buyable.display_icon_animation_id.value_or(kInvalidFrameDataId),
            .buyable_shop_owner_vid = entity.buyable.shop_owner_vid,
            .animate = presentation.animate,
            .animation_loop = presentation.animation_loop,
            .animation_finished = presentation.animation_finished,
            .animation_id = presentation.animation_id,
            .animation_frame = presentation.animation_frame,
            .animation_time = presentation.animation_time,
            .animation_speed = presentation.animation_speed,
        }
    );
}

void PatchPlayerState(State& state, const Entity& player) {
    network::ReplicatePlayerStatePatched(
        state,
        GameplayPlayerStatePatched{
            .player_vid = player.vid,
        }
    );
}

void PatchRunState(State& state) {
    network::ReplicateRunStatePatched(state);
}

void MarkEntityHeld(
    State& state,
    const Entity& holder,
    const Entity& held,
    AttachmentMode attachment_mode
) {
    network::ReplicateEntityHeld(
        state,
        GameplayEntityHeld{
            .holder_vid = holder.vid,
            .held_vid = held.vid,
            .attachment_mode = attachment_mode,
        }
    );
}

void MarkEntityDropped(
    State& state,
    const Entity& entity,
    std::optional<VID> dropped_by_vid
) {
    network::ReplicateEntityDropped(
        state,
        GameplayEntityDropped{
            .entity_vid = entity.vid,
            .dropped_by_vid = dropped_by_vid,
            .pos = entity.pos,
            .vel = entity.vel,
        }
    );
}

void MarkEntityThrown(State& state, const Entity& thrower, const Entity& thrown, Vec2 throw_velocity) {
    network::ReplicateEntityThrown(
        state,
        GameplayEntityThrown{
            .thrower_vid = thrower.vid,
            .entity_vid = thrown.vid,
            .pos = thrown.pos,
            .vel = throw_velocity,
        }
    );
}

void CommitEntityDamaged(
    State& state,
    const Entity& entity,
    DamageType damage_type,
    unsigned int amount,
    std::optional<VID> source_vid
) {
    const AnimationPresentationSnapshot presentation = CaptureDamagePresentation(entity);
    network::ReplicateEntityDamaged(
        state,
        GameplayEntityDamaged{
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
        }
    );
}

} // namespace splonks::world_ops

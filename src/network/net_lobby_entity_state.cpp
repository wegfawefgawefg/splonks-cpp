#include "network/net_lobby_internal.hpp"

#include "network/net_entity_links.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace splonks::network {

namespace {

constexpr float kReplicatedEntityStateMinDist = 0.01F;
constexpr std::uint32_t kCoordinatorRepairSnapshotIntervalFrames = 30;

bool ShouldConsiderEntityStatePatch(const State& state, const Entity& entity) {
    if (!entity.active || state.players.FindPlayerIdForEntity(entity.vid).has_value()) {
        return false;
    }
    if (!state.net_session.HasLocalAuthorityForEntity(entity.vid)) {
        return false;
    }
    return true;
}

bool HasMotionWorthReplicating(const State& state, const Entity& entity) {
    if (entity.dist_traveled_this_frame >= kReplicatedEntityStateMinDist) {
        return true;
    }
    if (std::abs(entity.vel.x) >= kReplicatedEntityStateMinDist ||
        std::abs(entity.vel.y) >= kReplicatedEntityStateMinDist) {
        return state.net_session.role == NetRole::Coordinator ||
               entity.pushable || entity.impassable || entity.crusher_pusher;
    }
    return false;
}

NetEntityId GetReplicatedEntityLinkId(State& state, const std::optional<VID>& vid) {
    if (!vid.has_value()) {
        return kInvalidNetEntityId;
    }
    const Entity* const linked_entity = state.entity_manager.GetEntity(*vid);
    if (linked_entity == nullptr || !linked_entity->active) {
        return kInvalidNetEntityId;
    }
    return GetOrAssignReplicatedEntityId(state, *vid);
}

EntityStatePatchedEvent MakeEntityStatePayload(State& state, const Entity& entity) {
    return EntityStatePatchedEvent{
        .entity_id = GetOrAssignReplicatedEntityId(state, entity.vid),
        .source_entity_id = kInvalidNetEntityId,
        .entity_a_id = GetReplicatedEntityLinkId(state, entity.entity_a),
        .holding_id = GetReplicatedEntityLinkId(state, entity.holding_vid),
        .held_by_id = GetReplicatedEntityLinkId(state, entity.held_by_vid),
        .back_id = GetReplicatedEntityLinkId(state, entity.back_vid),
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
        .counter_a = entity.counter_a,
        .counter_b = entity.counter_b,
        .point_a = entity.point_a,
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
        .buyable_active = static_cast<std::uint8_t>(entity.buyable.active ? 1 : 0),
        .buyable_display_quantity = entity.buyable.display_quantity,
        .buyable_display_icon_animation_id =
            entity.buyable.display_icon_animation_id.value_or(kInvalidFrameDataId),
        .buyable_shop_owner_id = GetReplicatedEntityLinkId(state, entity.buyable.shop_owner_vid),
        .animate = static_cast<std::uint8_t>(entity.frame_data_animator.animate ? 1 : 0),
        .animation_loop = static_cast<std::uint8_t>(entity.frame_data_animator.loop ? 1 : 0),
        .animation_finished = static_cast<std::uint8_t>(entity.frame_data_animator.finished ? 1 : 0),
        .animation_id = entity.frame_data_animator.animation_id,
        .animation_frame = static_cast<std::uint16_t>(entity.frame_data_animator.current_frame),
        .animation_time = entity.frame_data_animator.current_time,
        .animation_speed = entity.frame_data_animator.speed,
    };
}

NetReplicatedEntityStateSignature MakeStateSignature(const EntityStatePatchedEvent& payload) {
    return NetReplicatedEntityStateSignature{
        .entity_id = payload.entity_id,
        .entity_a_id = payload.entity_a_id,
        .holding_id = payload.holding_id,
        .held_by_id = payload.held_by_id,
        .back_id = payload.back_id,
        .buyable_shop_owner_id = payload.buyable_shop_owner_id,
        .buyable_display_icon_animation_id = payload.buyable_display_icon_animation_id,
        .animation_id = payload.animation_id,
        .pos_x = payload.pos.x,
        .pos_y = payload.pos.y,
        .vel_x = payload.vel.x,
        .vel_y = payload.vel.y,
        .acc_x = payload.acc.x,
        .acc_y = payload.acc.y,
        .counter_a = payload.counter_a,
        .counter_b = payload.counter_b,
        .rotation = payload.rotation,
        .animation_speed = payload.animation_speed,
        .health = payload.health,
        .stun_timer = payload.stun_timer,
        .projectile_contact_timer = payload.projectile_contact_timer,
        .buyable_display_quantity = payload.buyable_display_quantity,
        .animation_frame = payload.animation_frame,
        .condition = payload.condition,
        .grounded = payload.grounded,
        .active = payload.active,
        .has_physics = payload.has_physics,
        .can_collide = payload.can_collide,
        .can_apply_projectile_contact = payload.can_apply_projectile_contact,
        .facing = payload.facing,
        .ai_state = payload.ai_state,
        .wanted = payload.wanted,
        .attachment_mode = payload.attachment_mode,
        .buyable_active = payload.buyable_active,
        .animate = payload.animate,
        .animation_loop = payload.animation_loop,
        .animation_finished = payload.animation_finished,
    };
}

bool StateSignaturesEqual(
    const NetReplicatedEntityStateSignature& a,
    const NetReplicatedEntityStateSignature& b
) {
    return a.entity_id == b.entity_id &&
           a.entity_a_id == b.entity_a_id &&
           a.holding_id == b.holding_id &&
           a.held_by_id == b.held_by_id &&
           a.back_id == b.back_id &&
           a.buyable_shop_owner_id == b.buyable_shop_owner_id &&
           a.buyable_display_icon_animation_id == b.buyable_display_icon_animation_id &&
           a.animation_id == b.animation_id &&
           a.pos_x == b.pos_x &&
           a.pos_y == b.pos_y &&
           a.vel_x == b.vel_x &&
           a.vel_y == b.vel_y &&
           a.acc_x == b.acc_x &&
           a.acc_y == b.acc_y &&
           a.counter_a == b.counter_a &&
           a.counter_b == b.counter_b &&
           a.rotation == b.rotation &&
           a.animation_speed == b.animation_speed &&
           a.health == b.health &&
           a.stun_timer == b.stun_timer &&
           a.projectile_contact_timer == b.projectile_contact_timer &&
           a.buyable_display_quantity == b.buyable_display_quantity &&
           a.animation_frame == b.animation_frame &&
           a.condition == b.condition &&
           a.grounded == b.grounded &&
           a.active == b.active &&
           a.has_physics == b.has_physics &&
           a.can_collide == b.can_collide &&
           a.can_apply_projectile_contact == b.can_apply_projectile_contact &&
           a.facing == b.facing &&
           a.ai_state == b.ai_state &&
           a.wanted == b.wanted &&
           a.attachment_mode == b.attachment_mode &&
           a.buyable_active == b.buyable_active &&
           a.animate == b.animate &&
           a.animation_loop == b.animation_loop &&
           a.animation_finished == b.animation_finished;
}

bool UpdateStateCacheIfChanged(
    NetTransportRuntime& transport,
    const NetReplicatedEntityStateSignature& signature
) {
    for (NetReplicatedEntityStateCache& cache : transport.replicated_entity_state_cache) {
        if (cache.signature.entity_id != signature.entity_id) {
            continue;
        }
        if (StateSignaturesEqual(cache.signature, signature)) {
            return false;
        }
        cache.signature = signature;
        return true;
    }

    transport.replicated_entity_state_cache.push_back(NetReplicatedEntityStateCache{
        .signature = signature,
    });
    return true;
}

void PruneMissingStateCacheEntries(State& state, NetTransportRuntime& transport) {
    transport.replicated_entity_state_cache.erase(
        std::remove_if(
            transport.replicated_entity_state_cache.begin(),
            transport.replicated_entity_state_cache.end(),
            [&](const NetReplicatedEntityStateCache& cache) {
                return !state.net_session.FindLocalVid(cache.signature.entity_id).has_value();
            }
        ),
        transport.replicated_entity_state_cache.end()
    );
}

std::vector<NetEvent> BuildReplicatedEntityStatePatchEvents(
    State& state,
    NetTransportRuntime& transport
) {
    std::vector<NetEvent> events;
    for (const Entity& entity : state.entity_manager.entities) {
        if (!ShouldConsiderEntityStatePatch(state, entity)) {
            continue;
        }
        EntityStatePatchedEvent payload = MakeEntityStatePayload(state, entity);
        const bool changed = UpdateStateCacheIfChanged(transport, MakeStateSignature(payload));
        if (!changed && !HasMotionWorthReplicating(state, entity)) {
            continue;
        }
        NetEvent event;
        event.header = state.net_session.MakeLocalTransientEventHeader(state.frame);
        event.type = NetEventType::EntityStatePatched;
        event.payload = payload;
        events.push_back(event);
    }
    PruneMissingStateCacheEntries(state, transport);
    return events;
}

bool ShouldIncludeCoordinatorRepairEntityState(const State& state, const Entity& entity) {
    if (state.net_session.role != NetRole::Coordinator) {
        return false;
    }
    if (state.players.FindPlayerIdForEntity(entity.vid).has_value()) {
        return false;
    }
    // Inactive linked entities are intentionally repaired too. Entity state patches are
    // transient, so this is the safety net that clears remote ghosts after explosions,
    // pickups, rope deployment, or any other authoritative despawn path.
    return state.net_session.FindNetEntityId(entity.vid).has_value();
}

std::vector<NetEvent> BuildCoordinatorEntityRepairEvents(State& state) {
    std::vector<NetEvent> events;
    if (state.net_session.role != NetRole::Coordinator ||
        (state.frame % kCoordinatorRepairSnapshotIntervalFrames) != 0) {
        return events;
    }

    for (const Entity& entity : state.entity_manager.entities) {
        if (!ShouldIncludeCoordinatorRepairEntityState(state, entity)) {
            continue;
        }
        NetEvent event;
        event.header = state.net_session.MakeLocalTransientEventHeader(state.frame);
        event.type = NetEventType::EntityStatePatched;
        event.payload = MakeEntityStatePayload(state, entity);
        events.push_back(event);
    }
    return events;
}

} // namespace

void SendReplicatedEntityStatePatchesToAllRemotes(State& state, NetTransportRuntime& transport) {
    const std::vector<NetEvent> events = BuildReplicatedEntityStatePatchEvents(state, transport);
    if (events.empty()) {
        return;
    }
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEntityStateEvents(transport, remote.endpoint, events);
    }
}

void SendCoordinatorEntityRepairPatchesToAllRemotes(State& state, NetTransportRuntime& transport) {
    const std::vector<NetEvent> events = BuildCoordinatorEntityRepairEvents(state);
    if (events.empty()) {
        return;
    }
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEntityStateEvents(transport, remote.endpoint, events);
    }
}

} // namespace splonks::network

#include "network/net_lobby_internal.hpp"

#include "network/net_entity_links.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

namespace splonks::network {

namespace {

constexpr float kReplicatedEntityStateMinDist = 0.01F;
constexpr std::uint32_t kCoordinatorRepairSnapshotIntervalFrames = 30;

bool ShouldReplicateEntityStatePatch(const State& state, const Entity& entity) {
    if (!entity.active || state.players.FindPlayerIdForEntity(entity.vid).has_value()) {
        return false;
    }
    if (!state.net_session.HasLocalAuthorityForEntity(entity.vid)) {
        return false;
    }
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
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
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
        .animate = static_cast<std::uint8_t>(entity.frame_data_animator.animate ? 1 : 0),
        .animation_id = entity.frame_data_animator.animation_id,
        .animation_frame = static_cast<std::uint16_t>(entity.frame_data_animator.current_frame),
        .animation_time = entity.frame_data_animator.current_time,
        .animation_speed = entity.frame_data_animator.speed,
    };
}

std::vector<NetEvent> BuildReplicatedEntityStatePatchEvents(State& state) {
    std::vector<NetEvent> events;
    for (const Entity& entity : state.entity_manager.entities) {
        if (!ShouldReplicateEntityStatePatch(state, entity)) {
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
    const std::vector<NetEvent> events = BuildReplicatedEntityStatePatchEvents(state);
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

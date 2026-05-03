#include "entities/common/common.hpp"

#include "controls.hpp"
#include "entity/archetype.hpp"
#include "network/net_event.hpp"
#include "network/net_session.hpp"

namespace splonks::entities::common {

namespace {

Vec2 BuildThrowVelocity(const controls::ControlIntent& control) {
    Vec2 throw_vel = Vec2::New(0.0F, 0.0F);
    if (control.left) {
        throw_vel.x = -10.0F;
    } else if (control.right) {
        throw_vel.x = 10.0F;
    }
    if (control.up) {
        throw_vel.y = -10.0F;
    }
    if (control.down) {
        throw_vel.y = 10.0F;
    }
    if (!control.up && !control.down && (control.left || control.right)) {
        throw_vel.y = -2.0F;
    }
    return throw_vel;
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

} // namespace

void EmitReplicatedEntitySpawnedEvent(
    State& state,
    Entity& spawned_entity,
    std::optional<VID> held_by_vid
) {
    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }

    network::NetEntityId net_id = state.net_session.FindNetEntityId(spawned_entity.vid)
        .value_or(network::kInvalidNetEntityId);
    if (net_id == network::kInvalidNetEntityId) {
        net_id = state.net_session.AllocateLocalEntityId();
        state.net_session.LinkEntity(net_id, spawned_entity.vid);
    }
    state.net_session.SetEntityOwner(net_id, state.net_session.local_player_id);

    const Vec2 spawn_velocity = spawned_entity.vel + spawned_entity.acc;
    network::NetEntityId held_by_id = network::kInvalidNetEntityId;
    if (held_by_vid.has_value()) {
        if (const Entity* const holder = state.entity_manager.GetEntity(*held_by_vid)) {
            held_by_id = GetReplicatedEntityId(state, *holder);
        }
    }
    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntitySpawned;
    event.payload = network::EntitySpawnedEvent{
        .entity_id = net_id,
        .entity_type = spawned_entity.type_,
        .held_by_id = held_by_id,
        .pos = spawned_entity.pos,
        .vel = spawn_velocity,
        .owner = network::NetEntityOwner::Player(state.net_session.local_player_id),
        .counter_a = spawned_entity.counter_a,
        .counter_b = spawned_entity.counter_b,
        .use_pressed = spawned_entity.use_state.pressed,
    };
    state.net_session.EnqueueLocalEvent(event);
}

bool TrySpawnAndThrowEntityForToolUse(
    std::size_t thrower_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    ToolSlot& tool_slot,
    bool trigger_pressed,
    std::uint16_t cooldown_frames,
    std::uint32_t thrown_immunity_timer,
    void (*setup_entity)(Entity&),
    ToolThrowVelocityBuilder build_throw_velocity
) {
    (void)audio;
    if (!trigger_pressed) {
        return false;
    }

    const Entity& thrower = state.entity_manager.entities[thrower_idx];
    if (!tool_slot.active || tool_slot.count == 0 || tool_slot.cooldown > 0) {
        return false;
    }

    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(thrower, state);
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return false;
    }

    Entity* const spawned_entity = state.entity_manager.GetEntityMut(*vid);
    if (spawned_entity == nullptr) {
        return false;
    }

    setup_entity(*spawned_entity);
    spawned_entity->has_physics = true;
    spawned_entity->can_collide = true;
    UseEntity(*spawned_entity, thrower.vid, AttachmentMode::None);
    spawned_entity->thrown_by = thrower.vid;
    spawned_entity->thrown_immunity_timer = thrown_immunity_timer;
    const EntityArchetype& spawned_archetype = GetEntityArchetype(spawned_entity->type_);
    spawned_entity->can_apply_projectile_contact = spawned_archetype.can_apply_projectile_contact;
    spawned_entity->projectile_contact_damage_type = spawned_archetype.projectile_contact_damage_type;
    spawned_entity->projectile_contact_damage_amount = spawned_archetype.projectile_contact_damage_amount;
    spawned_entity->projectile_contact_timer = kProjectileContactDuration;
    const ToolThrowVelocityBuilder velocity_builder =
        build_throw_velocity == nullptr ? BuildThrowVelocity : build_throw_velocity;
    spawned_entity->SetCenter(thrower.GetCenter());
    spawned_entity->acc += velocity_builder(control) * thrower.throw_velocity_scale;
    state.UpdateSidForEntity(vid->id, graphics);
    EmitReplicatedEntitySpawnedEvent(state, *spawned_entity);

    tool_slot.count -= 1;
    tool_slot.cooldown = cooldown_frames;
    (void)PlayEntityCenterSoundEmitter(state, thrower, audio_asset_ids::Throw);
    return true;
}

} // namespace splonks::entities::common

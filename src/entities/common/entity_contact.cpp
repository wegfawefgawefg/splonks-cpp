#include "entities/common/common.hpp"

#include "entity/archetype.hpp"
#include "world_query.hpp"

namespace splonks::entities::common {

namespace {

bool ShouldDeduplicatePairThisTick(const ContactContext& context) {
    return context.direction == 0;
}

bool AreDirectlyAttached(const Entity& first, const Entity& second) {
    return (first.held_by_vid.has_value() && *first.held_by_vid == second.vid) ||
           (second.held_by_vid.has_value() && *second.held_by_vid == first.vid);
}

ContactResolution TryDispatchEntityEntityContactForParticipant(
    std::size_t participant_idx,
    std::size_t other_entity_idx,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (participant_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return ContactResolution{};
    }

    const Entity& participant = state.entity_manager.entities[participant_idx];
    const Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    if (!participant.active || !other_entity.active) {
        return ContactResolution{};
    }
    const EntityArchetype& participant_archetype = GetEntityArchetype(participant.type_);
    if (participant_archetype.entity_contact_cooldown_duration > 0 &&
        state.contact.HasContactCooldown(participant.vid, other_entity.vid)) {
        return ContactResolution{};
    }

    if (participant.crusher_pusher) {
        if (graphics != nullptr && audio != nullptr) {
            TryApplyCrusherPusherContact(
                participant_idx,
                other_entity_idx,
                context,
                state,
                *graphics,
                *audio
            );
        }
        return ContactResolution{};
    }

    ContactResolution resolution{};
    if (participant.can_stomp && graphics != nullptr && audio != nullptr &&
        TryApplyStompContactToEntity(participant_idx, other_entity_idx, state, *graphics, *audio)) {
        resolution.stop_sweep = true;
    }

    if (participant_archetype.on_entity_contact != nullptr) {
        const ContactResolution callback_resolution = participant_archetype.on_entity_contact(
            participant_idx,
            other_entity_idx,
            context,
            state,
            graphics,
            audio
        );
        resolution.blocks_movement |= callback_resolution.blocks_movement;
        resolution.stop_sweep |= callback_resolution.stop_sweep;
    }

    return resolution;
}

}

std::vector<VID> GatherTouchedEntityContactsForAabb(
    std::size_t entity_idx,
    const AABB& aabb,
    const Graphics& graphics,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return {};
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.active) {
        return {};
    }

    std::vector<VID> touched_vids;
    const Vec2 anchor = entity.GetCenter();
    for (const VID& other_vid : QueryEntitiesInAabb(state, aabb, entity.vid)) {
        const Entity* const other_entity = state.entity_manager.GetEntity(other_vid);
        if (other_entity == nullptr || !other_entity->active) {
            continue;
        }
        if (AreDirectlyAttached(entity, *other_entity)) {
            continue;
        }

        const AABB other_contact_aabb = GetNearestWorldAabb(
            state.stage,
            anchor,
            GetContactAabbForEntity(*other_entity, graphics)
        );
        if (!AabbsIntersect(aabb, other_contact_aabb)) {
            continue;
        }
        touched_vids.push_back(other_vid);
    }
    return touched_vids;
}

ContactResolution TryDispatchEntityEntityContactPair(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (entity_idx == other_entity_idx) {
        return ContactResolution{};
    }
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return ContactResolution{};
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    const Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    if (!entity.active || !other_entity.active) {
        return ContactResolution{};
    }
    if (ShouldDeduplicatePairThisTick(context) &&
        state.contact.HasEntityContactPairDispatchedThisTick(entity.vid, other_entity.vid)) {
        return ContactResolution{};
    }
    state.contact.RecordEntityContactPairDispatchedThisTick(entity.vid, other_entity.vid);

    ContactResolution result{};
    if (context.phase == ContactPhase::AttemptedBlocked && other_entity.impassable) {
        result.blocks_movement = true;
    }

    if (graphics != nullptr && audio != nullptr && context.phase == ContactPhase::SweptEntered) {
        const bool entity_projectile_hit = TryApplyProjectileContactToEntity(
            entity_idx,
            other_entity_idx,
            state,
            *graphics,
            *audio
        );
        const bool other_entity_projectile_hit = TryApplyProjectileContactToEntity(
            other_entity_idx,
            entity_idx,
            state,
            *graphics,
            *audio
        );
        result.stop_sweep |= entity_projectile_hit || other_entity_projectile_hit;
    }

    const EntityArchetype& entity_archetype = GetEntityArchetype(entity.type_);
    const ContactResolution entity_resolution = TryDispatchEntityEntityContactForParticipant(
        entity_idx, other_entity_idx, context, state, graphics, audio);
    result.blocks_movement |= entity_resolution.blocks_movement;
    result.stop_sweep |= entity_resolution.stop_sweep;
    if (entity_archetype.entity_contact_cooldown_duration > 0 &&
        (entity_resolution.blocks_movement || entity_resolution.stop_sweep)) {
        state.contact.AddContactCooldown(
            entity.vid,
            other_entity.vid,
            state.stage_frame,
            entity_archetype.entity_contact_cooldown_duration
        );
    }

    const EntityArchetype& other_archetype = GetEntityArchetype(other_entity.type_);
    const ContactResolution other_resolution = TryDispatchEntityEntityContactForParticipant(
        other_entity_idx, entity_idx, context, state, graphics, audio);
    result.blocks_movement |= other_resolution.blocks_movement;
    result.stop_sweep |= other_resolution.stop_sweep;
    if (other_archetype.entity_contact_cooldown_duration > 0 &&
        (other_resolution.blocks_movement || other_resolution.stop_sweep)) {
        state.contact.AddContactCooldown(
            other_entity.vid,
            entity.vid,
            state.stage_frame,
            other_archetype.entity_contact_cooldown_duration
        );
    }

    return result;
}

ContactResolution TryDispatchEntityEntityContacts(
    std::size_t entity_idx,
    const std::vector<VID>& touched_vids,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    ContactResolution aggregate{};
    for (const VID& other_vid : touched_vids) {
        const ContactResolution pair_resolution = TryDispatchEntityEntityContactPair(
            entity_idx, other_vid.id, context, state, graphics, audio);
        aggregate.blocks_movement |= pair_resolution.blocks_movement;
        aggregate.stop_sweep |= pair_resolution.stop_sweep;
    }
    return aggregate;
}

bool TryDispatchEntityEntityOverlapContacts(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio,
    const ContactContext& context
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.active) {
        return false;
    }

    const std::vector<VID> touched_vids = GatherTouchedEntityContactsForAabb(
        entity_idx,
        GetContactAabbForEntity(entity, graphics),
        graphics,
        state
    );
    return TryDispatchEntityEntityContacts(
               entity_idx,
               touched_vids,
               context,
               state,
               &graphics,
               &audio
           )
        .stop_sweep;
}

} // namespace splonks::entities::common

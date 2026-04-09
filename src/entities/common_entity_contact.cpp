#include "entities/common.hpp"

#include "entities/baseball_bat.hpp"
#include "entities/breakaway_container.hpp"

namespace splonks::entities::common {

namespace {

bool AabbsIntersect(const AABB& left, const AABB& right) {
    if (left.br.x < right.tl.x) {
        return false;
    }
    if (left.tl.x > right.br.x) {
        return false;
    }
    if (left.br.y < right.tl.y) {
        return false;
    }
    if (left.tl.y > right.br.y) {
        return false;
    }
    return true;
}

struct ParticipantContactDispatch {
    ContactResolution resolution;
    bool write_cooldown = false;
    std::uint32_t cooldown_duration = 0;
};

std::optional<ParticipantContactDispatch> GetEntityEntityContactCooldownSpec(
    const Entity& participant,
    const ContactContext& context,
    const Graphics* graphics,
    Audio* audio
) {
    (void)context;
    switch (participant.type_) {
    case EntityType::BaseballBat:
        if (graphics == nullptr || audio == nullptr) {
            return std::nullopt;
        }
        return ParticipantContactDispatch{
            .resolution = ContactResolution{},
            .write_cooldown = true,
            .cooldown_duration = baseball_bat::kBatContactCooldownFrames,
        };
    default:
        return std::nullopt;
    }
}

ContactResolution TryDispatchEntityEntityContactByType(
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

    switch (participant.type_) {
    case EntityType::BaseballBat: {
        const bool applied = baseball_bat::TryApplyBatContactToEntity(
            participant_idx, other_entity_idx, state, *graphics, *audio);
        return ContactResolution{
            .blocks_movement = false,
            .stop_sweep = applied,
        };
    }
    case EntityType::Pot:
    case EntityType::Box:
        if (!breakaway_container::TryApplyBreakawayContainerImpact(
                participant_idx, context, state)) {
            return ContactResolution{};
        }
        return ContactResolution{
            .blocks_movement = false,
            .stop_sweep = true,
        };
    case EntityType::None:
    case EntityType::Player:
    case EntityType::Block:
    case EntityType::GhostBall:
    case EntityType::Bat:
    case EntityType::Rock:
    case EntityType::MouseTrailer:
    case EntityType::JetPack:
    case EntityType::Bomb:
    case EntityType::Gold:
    case EntityType::GoldStack:
    case EntityType::Rope:
        return ContactResolution{};
    }

    return ContactResolution{};
}

ParticipantContactDispatch TryDispatchEntityEntityContactForParticipant(
    std::size_t participant_idx,
    std::size_t other_entity_idx,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (participant_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return ParticipantContactDispatch{};
    }

    const Entity& participant = state.entity_manager.entities[participant_idx];
    const Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    const std::optional<ParticipantContactDispatch> cooldown_spec =
        GetEntityEntityContactCooldownSpec(participant, context, graphics, audio);
    if (cooldown_spec.has_value() &&
        state.HasContactCooldown(participant.vid, other_entity.vid)) {
        return ParticipantContactDispatch{};
    }

    const ContactResolution resolution = TryDispatchEntityEntityContactByType(
        participant_idx, other_entity_idx, context, state, graphics, audio);
    ParticipantContactDispatch dispatch{
        .resolution = resolution,
    };
    if (cooldown_spec.has_value()) {
        dispatch.write_cooldown = true;
        dispatch.cooldown_duration = cooldown_spec->cooldown_duration;
    }
    return dispatch;
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
    for (const VID& other_vid : state.sid.QueryExclude(aabb.tl, aabb.br, entity.vid)) {
        const Entity* const other_entity = state.entity_manager.GetEntity(other_vid);
        if (other_entity == nullptr || !other_entity->active) {
            continue;
        }

        const AABB other_contact_aabb = GetContactAabbForEntity(*other_entity, graphics);
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
    if (state.HasEntityContactPairDispatchedThisTick(entity.vid, other_entity.vid)) {
        return ContactResolution{};
    }
    state.RecordEntityContactPairDispatchedThisTick(entity.vid, other_entity.vid);

    ContactResolution result{};
    if (context.phase == ContactPhase::AttemptedBlocked && other_entity.impassable) {
        result.blocks_movement = true;
    }

    const ParticipantContactDispatch entity_dispatch = TryDispatchEntityEntityContactForParticipant(
        entity_idx, other_entity_idx, context, state, graphics, audio);
    result.blocks_movement |= entity_dispatch.resolution.blocks_movement;
    result.stop_sweep |= entity_dispatch.resolution.stop_sweep;
    if (entity_dispatch.write_cooldown) {
        state.AddContactCooldown(
            entity.vid,
            other_entity.vid,
            entity_dispatch.cooldown_duration
        );
    }

    const ParticipantContactDispatch other_dispatch = TryDispatchEntityEntityContactForParticipant(
        other_entity_idx, entity_idx, context, state, graphics, audio);
    result.blocks_movement |= other_dispatch.resolution.blocks_movement;
    result.stop_sweep |= other_dispatch.resolution.stop_sweep;
    if (other_dispatch.write_cooldown) {
        state.AddContactCooldown(
            other_entity.vid,
            entity.vid,
            other_dispatch.cooldown_duration
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
    Audio& audio
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
               ContactContext{
                   .phase = ContactPhase::SweptEntered,
                   .has_impact = false,
               },
               state,
               &graphics,
               &audio
           )
        .stop_sweep;
}

} // namespace splonks::entities::common

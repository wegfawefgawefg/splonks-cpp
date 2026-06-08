#include "ents/common/common.hpp"

#include "ent/spec.hpp"
#include "world_query.hpp"

namespace splonks::ents::common {

namespace {

bool ShouldDeduplicatePairThisTick(const ContactContext& context) {
    return context.direction == 0;
}

bool AreDirectlyAttached(const Ent& first, const Ent& second) {
    return (first.held_by_vid.has_value() && *first.held_by_vid == second.vid) ||
           (second.held_by_vid.has_value() && *second.held_by_vid == first.vid);
}

ContactResult TryDispatchEntEntContactForParticipant(
    std::size_t participant_idx,
    std::size_t other_ent_idx,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (participant_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return ContactResult{};
    }

    const Ent& participant = state.ents.ents[participant_idx];
    const Ent& other_ent = state.ents.ents[other_ent_idx];
    if (!participant.active || !other_ent.active) {
        return ContactResult{};
    }
    const EntSpec& participant_spec = GetEntSpec(participant.type_);
    if (participant_spec.ent_contact_cooldown_duration > 0 &&
        state.contact.HasContactCooldown(participant.vid, other_ent.vid)) {
        return ContactResult{};
    }

    if (participant.crusher_pusher) {
        if (graphics != nullptr && audio != nullptr) {
            TryApplyCrusherPusherContact(
                participant_idx,
                other_ent_idx,
                context,
                state,
                *graphics,
                *audio
            );
        }
        return ContactResult{};
    }

    ContactResult resolution{};
    if (participant.can_stomp && graphics != nullptr && audio != nullptr &&
        TryApplyStompContactToEnt(participant_idx, other_ent_idx, state, *graphics, *audio)) {
        resolution.stop_sweep = true;
    }

    if (participant_spec.on_ent_contact != nullptr) {
        const ContactResult callback_resolution = participant_spec.on_ent_contact(
            participant_idx,
            other_ent_idx,
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

std::vector<VID> GatherTouchedEntContactsForAabb(
    std::size_t ent_idx,
    const AABB& aabb,
    const Graphics& graphics,
    State& state
) {
    if (ent_idx >= state.ents.ents.size()) {
        return {};
    }

    const Ent& ent = state.ents.ents[ent_idx];
    if (!ent.active) {
        return {};
    }

    std::vector<VID> touched_vids;
    const Vec2 anchor = ent.GetCenter();
    for (const VID& other_vid : QueryEntsInAabb(state, aabb, ent.vid)) {
        const Ent* const other_ent = state.ents.GetEnt(other_vid);
        if (other_ent == nullptr || !other_ent->active) {
            continue;
        }
        if (AreDirectlyAttached(ent, *other_ent)) {
            continue;
        }

        const AABB other_contact_aabb = GetNearestWorldAabb(
            state.stage,
            anchor,
            GetRenderContactAabbForEnt(*other_ent, graphics)
        );
        if (!AabbsIntersect(aabb, other_contact_aabb)) {
            continue;
        }
        touched_vids.push_back(other_vid);
    }
    return touched_vids;
}

ContactResult TryDispatchEntEntContactPair(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (ent_idx == other_ent_idx) {
        return ContactResult{};
    }
    if (ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return ContactResult{};
    }

    const Ent& ent = state.ents.ents[ent_idx];
    const Ent& other_ent = state.ents.ents[other_ent_idx];
    if (!ent.active || !other_ent.active) {
        return ContactResult{};
    }
    if (ShouldDeduplicatePairThisTick(context) &&
        state.contact.HasEntContactPairDispatchedThisTick(ent.vid, other_ent.vid)) {
        return ContactResult{};
    }
    state.contact.RecordEntContactPairDispatchedThisTick(ent.vid, other_ent.vid);

    ContactResult result{};
    if (context.phase == ContactPhase::AttemptedBlocked && other_ent.impassable) {
        result.blocks_movement = true;
    }

    if (graphics != nullptr && audio != nullptr && context.phase == ContactPhase::SweptEntered) {
        const bool ent_proj_hit = TryApplyProjContactToEnt(
            ent_idx,
            other_ent_idx,
            state,
            *graphics,
            *audio
        );
        const bool other_ent_proj_hit = TryApplyProjContactToEnt(
            other_ent_idx,
            ent_idx,
            state,
            *graphics,
            *audio
        );
        result.stop_sweep |= ent_proj_hit || other_ent_proj_hit;
    }

    const EntSpec& ent_spec = GetEntSpec(ent.type_);
    const ContactResult ent_resolution = TryDispatchEntEntContactForParticipant(
        ent_idx, other_ent_idx, context, state, graphics, audio);
    result.blocks_movement |= ent_resolution.blocks_movement;
    result.stop_sweep |= ent_resolution.stop_sweep;
    if (ent_spec.ent_contact_cooldown_duration > 0 &&
        (ent_resolution.blocks_movement || ent_resolution.stop_sweep)) {
        state.contact.AddContactCooldown(
            ent.vid,
            other_ent.vid,
            state.stage_frame,
            ent_spec.ent_contact_cooldown_duration
        );
    }

    const EntSpec& other_spec = GetEntSpec(other_ent.type_);
    const ContactResult other_resolution = TryDispatchEntEntContactForParticipant(
        other_ent_idx, ent_idx, context, state, graphics, audio);
    result.blocks_movement |= other_resolution.blocks_movement;
    result.stop_sweep |= other_resolution.stop_sweep;
    if (other_spec.ent_contact_cooldown_duration > 0 &&
        (other_resolution.blocks_movement || other_resolution.stop_sweep)) {
        state.contact.AddContactCooldown(
            other_ent.vid,
            ent.vid,
            state.stage_frame,
            other_spec.ent_contact_cooldown_duration
        );
    }

    return result;
}

ContactResult TryDispatchEntEntContacts(
    std::size_t ent_idx,
    const std::vector<VID>& touched_vids,
    const ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    ContactResult aggregate{};
    for (const VID& other_vid : touched_vids) {
        const ContactResult pair_resolution = TryDispatchEntEntContactPair(
            ent_idx, other_vid.id, context, state, graphics, audio);
        aggregate.blocks_movement |= pair_resolution.blocks_movement;
        aggregate.stop_sweep |= pair_resolution.stop_sweep;
    }
    return aggregate;
}

bool TryDispatchEntEntOverlapContacts(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio,
    const ContactContext& context
) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& ent = state.ents.ents[ent_idx];
    if (!ent.active) {
        return false;
    }

    const std::vector<VID> touched_vids = GatherTouchedEntContactsForAabb(
        ent_idx,
        GetRenderContactAabbForEnt(ent, graphics),
        graphics,
        state
    );
    return TryDispatchEntEntContacts(
               ent_idx,
               touched_vids,
               context,
               state,
               &graphics,
               &audio
           )
        .stop_sweep;
}

} // namespace splonks::ents::common

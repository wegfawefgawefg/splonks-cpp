#include "contact_bookkeeping.hpp"

namespace splonks {

namespace {

EntContactDispatchEntry NormalizeEntContactDispatchEntry(
    const VID& first_vid,
    const VID& second_vid
) {
    if (first_vid.id < second_vid.id) {
        return EntContactDispatchEntry{
            .first_vid = first_vid,
            .second_vid = second_vid,
        };
    }
    if (first_vid.id > second_vid.id) {
        return EntContactDispatchEntry{
            .first_vid = second_vid,
            .second_vid = first_vid,
        };
    }
    if (first_vid.version <= second_vid.version) {
        return EntContactDispatchEntry{
            .first_vid = first_vid,
            .second_vid = second_vid,
        };
    }
    return EntContactDispatchEntry{
        .first_vid = second_vid,
        .second_vid = first_vid,
    };
}

ProjBodyImpactCooldownEntry MakeProjBodyImpactCooldownEntry(
    const VID& source_vid,
    const VID& target_vid,
    std::uint32_t expires_on_stage_frame
) {
    return ProjBodyImpactCooldownEntry{
        .first_vid = source_vid,
        .second_vid = target_vid,
        .expires_on_stage_frame = expires_on_stage_frame,
    };
}

} // namespace

void ContactBookkeeping::ClearEntContactDispatchesThisTick() {
    ent_contact_dispatches_this_tick.clear();
}

bool ContactBookkeeping::HasEntContactPairDispatchedThisTick(
    const VID& first_vid,
    const VID& second_vid
) const {
    const EntContactDispatchEntry normalized =
        NormalizeEntContactDispatchEntry(first_vid, second_vid);
    for (const EntContactDispatchEntry& entry : ent_contact_dispatches_this_tick) {
        if (entry.first_vid == normalized.first_vid && entry.second_vid == normalized.second_vid) {
            return true;
        }
    }
    return false;
}

void ContactBookkeeping::RecordEntContactPairDispatchedThisTick(
    const VID& first_vid,
    const VID& second_vid
) {
    const EntContactDispatchEntry normalized =
        NormalizeEntContactDispatchEntry(first_vid, second_vid);
    if (HasEntContactPairDispatchedThisTick(normalized.first_vid, normalized.second_vid)) {
        return;
    }
    ent_contact_dispatches_this_tick.push_back(normalized);
}

void ContactBookkeeping::StepContactCooldowns(std::uint32_t stage_frame) {
    std::vector<ContactCooldownEntry> kept_cooldowns;
    kept_cooldowns.reserve(contact_cooldowns.size());
    for (const ContactCooldownEntry& entry : contact_cooldowns) {
        if (entry.expires_on_stage_frame > stage_frame) {
            kept_cooldowns.push_back(entry);
        }
    }
    contact_cooldowns = std::move(kept_cooldowns);
}

bool ContactBookkeeping::HasContactCooldown(
    const VID& source_vid,
    const VID& target_vid
) const {
    for (const ContactCooldownEntry& entry : contact_cooldowns) {
        if (entry.source_vid == source_vid && entry.target_vid == target_vid) {
            return true;
        }
    }
    return false;
}

void ContactBookkeeping::AddContactCooldown(
    const VID& source_vid,
    const VID& target_vid,
    std::uint32_t stage_frame,
    std::uint32_t duration
) {
    for (ContactCooldownEntry& entry : contact_cooldowns) {
        if (entry.source_vid == source_vid && entry.target_vid == target_vid) {
            entry.expires_on_stage_frame = stage_frame + duration;
            return;
        }
    }

    contact_cooldowns.push_back(ContactCooldownEntry{
        .source_vid = source_vid,
        .target_vid = target_vid,
        .expires_on_stage_frame = stage_frame + duration,
    });
}

void ContactBookkeeping::StepInteractionCooldowns(std::uint32_t stage_frame) {
    std::vector<InteractionCooldownEntry> kept_cooldowns;
    kept_cooldowns.reserve(interaction_cooldowns.size());
    for (const InteractionCooldownEntry& entry : interaction_cooldowns) {
        if (entry.expires_on_stage_frame > stage_frame) {
            kept_cooldowns.push_back(entry);
        }
    }
    interaction_cooldowns = std::move(kept_cooldowns);
}

bool ContactBookkeeping::HasInteractionCooldown(
    const VID& source_vid,
    const VID& target_vid,
    InteractionCooldownKind kind
) const {
    for (const InteractionCooldownEntry& entry : interaction_cooldowns) {
        if (entry.source_vid == source_vid && entry.target_vid == target_vid && entry.kind == kind) {
            return true;
        }
    }
    return false;
}

void ContactBookkeeping::AddInteractionCooldown(
    const VID& source_vid,
    const VID& target_vid,
    InteractionCooldownKind kind,
    std::uint32_t stage_frame,
    std::uint32_t duration
) {
    for (InteractionCooldownEntry& entry : interaction_cooldowns) {
        if (entry.source_vid == source_vid && entry.target_vid == target_vid && entry.kind == kind) {
            entry.expires_on_stage_frame = stage_frame + duration;
            return;
        }
    }

    interaction_cooldowns.push_back(InteractionCooldownEntry{
        .source_vid = source_vid,
        .target_vid = target_vid,
        .kind = kind,
        .expires_on_stage_frame = stage_frame + duration,
    });
}

void ContactBookkeeping::StepProjBodyImpactCooldowns(std::uint32_t stage_frame) {
    std::vector<ProjBodyImpactCooldownEntry> kept_cooldowns;
    kept_cooldowns.reserve(proj_body_impact_cooldowns.size());
    for (const ProjBodyImpactCooldownEntry& entry : proj_body_impact_cooldowns) {
        if (entry.expires_on_stage_frame > stage_frame) {
            kept_cooldowns.push_back(entry);
        }
    }
    proj_body_impact_cooldowns = std::move(kept_cooldowns);
}

bool ContactBookkeeping::HasProjBodyImpactCooldown(
    const VID& first_vid,
    const VID& second_vid
) const {
    const ProjBodyImpactCooldownEntry ordered =
        MakeProjBodyImpactCooldownEntry(first_vid, second_vid, 0);
    for (const ProjBodyImpactCooldownEntry& entry : proj_body_impact_cooldowns) {
        if (entry.first_vid == ordered.first_vid && entry.second_vid == ordered.second_vid) {
            return true;
        }
    }
    return false;
}

void ContactBookkeeping::AddProjBodyImpactCooldown(
    const VID& first_vid,
    const VID& second_vid,
    std::uint32_t stage_frame,
    std::uint32_t duration
) {
    const ProjBodyImpactCooldownEntry ordered =
        MakeProjBodyImpactCooldownEntry(first_vid, second_vid, stage_frame + duration);
    for (ProjBodyImpactCooldownEntry& entry : proj_body_impact_cooldowns) {
        if (entry.first_vid == ordered.first_vid && entry.second_vid == ordered.second_vid) {
            entry.expires_on_stage_frame = ordered.expires_on_stage_frame;
            return;
        }
    }
    proj_body_impact_cooldowns.push_back(ordered);
}

} // namespace splonks

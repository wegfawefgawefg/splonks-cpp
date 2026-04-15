#pragma once

#include "sid.hpp"

#include <cstdint>
#include <vector>

namespace splonks {

struct ContactCooldownEntry {
    VID source_vid;
    VID target_vid;
    std::uint32_t expires_on_stage_frame = 0;
};

enum class InteractionCooldownKind {
    Harm,
};

struct InteractionCooldownEntry {
    VID source_vid;
    VID target_vid;
    InteractionCooldownKind kind = InteractionCooldownKind::Harm;
    std::uint32_t expires_on_stage_frame = 0;
};

struct EntityContactDispatchEntry {
    VID first_vid;
    VID second_vid;
};

struct ProjectileBodyImpactCooldownEntry {
    VID first_vid;
    VID second_vid;
    std::uint32_t expires_on_stage_frame = 0;
};

struct ContactBookkeeping {
    std::vector<ContactCooldownEntry> contact_cooldowns;
    std::vector<InteractionCooldownEntry> interaction_cooldowns;
    std::vector<EntityContactDispatchEntry> entity_contact_dispatches_this_tick;
    std::vector<ProjectileBodyImpactCooldownEntry> projectile_body_impact_cooldowns;

    void ClearEntityContactDispatchesThisTick();
    bool HasEntityContactPairDispatchedThisTick(
        const VID& first_vid,
        const VID& second_vid
    ) const;
    void RecordEntityContactPairDispatchedThisTick(
        const VID& first_vid,
        const VID& second_vid
    );
    void StepContactCooldowns(std::uint32_t stage_frame);
    void StepInteractionCooldowns(std::uint32_t stage_frame);
    bool HasContactCooldown(
        const VID& source_vid,
        const VID& target_vid
    ) const;
    void AddContactCooldown(
        const VID& source_vid,
        const VID& target_vid,
        std::uint32_t stage_frame,
        std::uint32_t duration
    );
    bool HasInteractionCooldown(
        const VID& source_vid,
        const VID& target_vid,
        InteractionCooldownKind kind
    ) const;
    void AddInteractionCooldown(
        const VID& source_vid,
        const VID& target_vid,
        InteractionCooldownKind kind,
        std::uint32_t stage_frame,
        std::uint32_t duration
    );
    void StepProjectileBodyImpactCooldowns(std::uint32_t stage_frame);
    bool HasProjectileBodyImpactCooldown(
        const VID& first_vid,
        const VID& second_vid
    ) const;
    void AddProjectileBodyImpactCooldown(
        const VID& first_vid,
        const VID& second_vid,
        std::uint32_t stage_frame,
        std::uint32_t duration
    );
};

} // namespace splonks

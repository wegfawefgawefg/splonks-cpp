#pragma once

#include "ent.hpp"

namespace splonks::ents::common {

struct KnockbackSpec {
    FxVec2 velocity = FxVec2::zero();
    bool clear_velocity = true;
    bool clear_acceleration = true;
    std::optional<VID> thrown_by = std::nullopt;
    std::uint32_t thrown_immunity_timer = 0;
    DamageType proj_contact_damage_type = DamageType::Attack;
    std::uint32_t proj_contact_damage_amount = 1;
    std::uint32_t proj_contact_duration = 0;
};

void ApplyKnockback(Ent& target, const KnockbackSpec& spec);

} // namespace splonks::ents::common

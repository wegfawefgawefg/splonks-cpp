#pragma once

#include "entity.hpp"

namespace splonks::entities::common {

struct KnockbackSpec {
    Vec2 velocity = Vec2::New(0.0F, 0.0F);
    bool clear_velocity = true;
    bool clear_acceleration = true;
    std::optional<VID> thrown_by = std::nullopt;
    std::uint32_t thrown_immunity_timer = 0;
    DamageType projectile_contact_damage_type = DamageType::Attack;
    unsigned int projectile_contact_damage_amount = 1;
    std::uint32_t projectile_contact_duration = 0;
};

void ApplyKnockback(Entity& target, const KnockbackSpec& spec);

} // namespace splonks::entities::common

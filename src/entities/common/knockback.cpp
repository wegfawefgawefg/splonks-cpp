#include "entities/common/knockback.hpp"

namespace splonks::entities::common {

void ApplyKnockback(Entity& target, const KnockbackSpec& spec) {
    if (spec.clear_velocity) {
        target.vel = spec.velocity;
    } else {
        target.vel += spec.velocity;
    }

    if (spec.clear_acceleration) {
        target.acc = Vec2::New(0.0F, 0.0F);
    }

    if (spec.thrown_by.has_value()) {
        target.thrown_by = spec.thrown_by;
        target.thrown_immunity_timer = spec.thrown_immunity_timer;
    }
    if (spec.projectile_contact_duration > 0) {
        target.projectile_contact_damage_type = spec.projectile_contact_damage_type;
        target.projectile_contact_damage_amount = spec.projectile_contact_damage_amount;
        target.projectile_contact_timer = spec.projectile_contact_duration;
    }
}

} // namespace splonks::entities::common

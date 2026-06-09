#include "ents/common/knockback.hpp"

namespace splonks::ents::common {

void ApplyKnockback(Ent& target, const KnockbackSpec& spec) {
    if (spec.clear_velocity) {
        target.vel = spec.velocity;
    } else {
        target.vel += spec.velocity;
    }

    if (spec.clear_acceleration) {
        target.acc = sim::FxVec2::zero();
    }

    if (spec.thrown_by.has_value()) {
        target.thrown_by = spec.thrown_by;
        target.thrown_immunity_timer = spec.thrown_immunity_timer;
    }
    if (spec.proj_contact_duration > 0) {
        target.proj_contact_damage_type = spec.proj_contact_damage_type;
        target.proj_contact_damage_amount = spec.proj_contact_damage_amount;
        target.proj_contact_timer = spec.proj_contact_duration;
    }
}

} // namespace splonks::ents::common

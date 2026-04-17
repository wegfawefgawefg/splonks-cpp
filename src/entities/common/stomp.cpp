#include "entities/common/common.hpp"
#include "world_query.hpp"

#include <algorithm>

namespace splonks::entities::common {

namespace {

constexpr float kStompHeadHeight = 1.0F;
constexpr float kStompBounceVelocityY = -4.5F;
constexpr float kStompVictimKnockbackVelocityY = -1.0F;
constexpr float kStompVictimKnockbackVelocityX = 1.0F;

bool CanEntityAttemptStomp(const Entity& stomper) {
    if (!stomper.active) {
        return false;
    }
    if (!stomper.can_stomp) {
        return false;
    }
    if (stomper.condition != EntityCondition::Normal) {
        return false;
    }
    if (stomper.vel.y <= 0.0F) {
        return false;
    }
    if (stomper.held_by_vid.has_value()) {
        return false;
    }
    if (HasMovementFlag(stomper, EntityMovementFlag::Hanging)) {
        return false;
    }
    return true;
}

bool CanEntityBeStomped(const Entity& target) {
    if (!target.active) {
        return false;
    }
    if (!target.can_be_stomped) {
        return false;
    }
    if (target.impassable || !target.can_collide) {
        return false;
    }
    if (target.condition != EntityCondition::Normal) {
        return false;
    }
    if (HasMovementFlag(target, EntityMovementFlag::Hanging)) {
        return false;
    }
    return true;
}

} // namespace

bool TryApplyStompContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& stomper = state.entity_manager.entities[entity_idx];
    Entity* const stomped = state.entity_manager.GetEntityMut(state.entity_manager.entities[other_entity_idx].vid);
    if (stomped == nullptr) {
        return false;
    }

    if (!CanEntityAttemptStomp(stomper)) {
        return false;
    }
    if (!CanEntityBeStomped(*stomped)) {
        return false;
    }

    const AABB stomper_aabb = GetContactAabbForEntity(stomper, graphics);
    const AABB stomped_aabb = GetNearestWorldAabb(
        state.stage,
        stomper.GetCenter(),
        GetContactAabbForEntity(*stomped, graphics)
    );
    const float stomped_head_band_bottom = stomped_aabb.tl.y + kStompHeadHeight;
    if (stomper_aabb.br.y > stomped_head_band_bottom) {
        return false;
    }

    audio.PlaySoundEffect(SoundEffect::Jump);

    const DamageResult damage_result = TryDamageEntity(
        stomped->vid.id,
        state,
        audio,
        DamageType::JumpOn,
        1
    );
    if (stomped->can_be_stunned) {
        stomped->condition = EntityCondition::Stunned;
        stomped->stun_timer = kDefaultStunTimer;
    }
    if (damage_result == DamageResult::Died || damage_result == DamageResult::Hurt) {
        const Vec2 stomp_delta = GetNearestWorldDelta(state.stage, stomper.GetCenter(), stomped->GetCenter());
        const float stomp_knockback_x =
            stomp_delta.x < 0.0F ? -kStompVictimKnockbackVelocityX : kStompVictimKnockbackVelocityX;
        ApplyKnockback(
            *stomped,
            KnockbackSpec{
                .velocity = Vec2::New(stomp_knockback_x, kStompVictimKnockbackVelocityY),
                .clear_velocity = true,
                .clear_acceleration = true,
                .thrown_by = stomper.vid,
                .thrown_immunity_timer = kThrownByImmunityDuration,
                .projectile_contact_damage_type = DamageType::Attack,
                .projectile_contact_damage_amount = 1,
                .projectile_contact_duration = kProjectileContactDuration,
            }
        );
    }
    state.contact.AddInteractionCooldown(
        stomped->vid,
        stomper.vid,
        InteractionCooldownKind::Harm,
        state.stage_frame,
        1
    );

    stomper.vel.y = kStompBounceVelocityY;
    return true;
}

} // namespace splonks::entities::common

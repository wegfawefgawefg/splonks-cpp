#include "entities/common/common.hpp"
#include "controls.hpp"
#include "world_query.hpp"

#include <algorithm>

namespace splonks::entities::common {

namespace {

constexpr float kStompHeadHeight = 1.0F;
constexpr float kStompShortBounceVelocityY = -3.0F;
constexpr float kStompHeldJumpBounceVelocityY = -4.5F;
constexpr float kStompVictimKnockbackVelocityY = -1.0F;
constexpr float kStompVictimKnockbackVelocityX = 1.0F;
constexpr unsigned int kStompDamage = 1;
constexpr float kSpringShoeMovementSoundVolume = 0.15F;

bool CanEntityAttemptStomp(const Entity& stomper, const State& state) {
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
    if (GetModifiedEffectValue(stomper, EffectModifierTarget::StompDamageScale, 1.0F, &state) <= 0.0F) {
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

void ApplyStompBounce(Entity& stomper, State& state) {
    const controls::ControlIntent control = controls::GetControlIntentForEntity(stomper, state);
    const float base_bounce_impulse = control.jump
        ? -kStompHeldJumpBounceVelocityY
        : -kStompShortBounceVelocityY;
    const float bounce_impulse =
        GetModifiedEffectValue(stomper, EffectModifierTarget::StompBounceImpulse, base_bounce_impulse, &state);
    stomper.vel.y = -bounce_impulse;
}

} // namespace

bool TryApplyStompContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& stomper = state.entity_manager.entities[entity_idx];
    Entity* const stomped = state.entity_manager.GetEntityMut(state.entity_manager.entities[other_entity_idx].vid);
    if (stomped == nullptr) {
        return false;
    }

    if (!CanEntityAttemptStomp(stomper, state)) {
        return false;
    }
    if (!CanEntityBeStomped(*stomped)) {
        return false;
    }
    if (state.contact.HasInteractionCooldown(
            stomper.vid,
            stomped->vid,
            InteractionCooldownKind::Harm
        )) {
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

    (void)PlayEntityCenterSoundEmitter(state, stomper, audio_asset_ids::Jump);
    const bool has_spring_shoes = HasEffect(stomper, EffectId::SpringShoes);
    if (has_spring_shoes) {
        (void)PlayEntityCenterSoundEmitter(
            state,
            stomper,
            audio_asset_ids::SpringShoe,
            AudioEmitterPlayParams{.volume_scale = kSpringShoeMovementSoundVolume}
        );
    }

    if (IsPlayerLikeEntityType(stomper.type_) && IsPlayerLikeEntityType(stomped->type_)) {
        ApplyStompBounce(stomper, state);
        return true;
    }

    const float base_stomp_damage = GetModifiedEffectValue(
        stomper,
        EffectModifierTarget::StompDamage,
        static_cast<float>(kStompDamage),
        &state
    );
    const float stomp_damage_scale =
        GetModifiedEffectValue(stomper, EffectModifierTarget::StompDamageScale, 1.0F, &state);
    const unsigned int stomp_damage =
        static_cast<unsigned int>(std::max(0.0F, base_stomp_damage * stomp_damage_scale));
    if (stomp_damage == 0) {
        return false;
    }
    const Vec2 stomp_delta = GetNearestWorldDelta(state.stage, stomper.GetCenter(), stomped->GetCenter());
    const float stomp_knockback_x =
        stomp_delta.x < 0.0F ? -kStompVictimKnockbackVelocityX : kStompVictimKnockbackVelocityX;
    const KnockbackSpec knockback{
        .velocity = Vec2::New(stomp_knockback_x, kStompVictimKnockbackVelocityY),
        .clear_velocity = true,
        .clear_acceleration = true,
        .thrown_by = stomper.vid,
        .thrown_immunity_timer = kThrownByImmunityDuration,
        .projectile_contact_damage_type = DamageType::Attack,
        .projectile_contact_damage_amount = 1,
        .projectile_contact_duration = kProjectileContactDuration,
    };
    const DamageResult damage_result = TryHitEntity(
        stomped->vid.id,
        state,
        audio,
        DamageType::JumpOn,
        stomp_damage,
        HitOptions{
            .source_vid = stomper.vid,
            .knockback = knockback,
            .allow_remote_player_target = true,
        }
    );
    if (damage_result != DamageResult::Requested && stomped->can_be_stunned) {
        stomped->condition = EntityCondition::Stunned;
        stomped->stun_timer = kDefaultStunTimer;
    }
    state.contact.AddInteractionCooldown(
        stomped->vid,
        stomper.vid,
        InteractionCooldownKind::Harm,
        state.stage_frame,
        1
    );

    ApplyStompBounce(stomper, state);
    return true;
}

} // namespace splonks::entities::common

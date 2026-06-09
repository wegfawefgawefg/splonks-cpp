#include "ents/common/common.hpp"
#include "controls.hpp"
#include "world_query.hpp"

#include <algorithm>

namespace splonks::ents::common {

namespace {

constexpr float kStompHeadHeight = 1.0F;
constexpr float kStompShortBounceVelocityY = -3.0F;
constexpr float kStompHeldJumpBounceVelocityY = -4.5F;
constexpr float kStompVictimKnockbackVelocityY = -1.0F;
constexpr float kStompVictimKnockbackVelocityX = 1.0F;
constexpr std::uint32_t kStompDamage = 1;
constexpr float kSpringShoeMovementSoundVolume = 0.15F;

bool CanEntAttemptStomp(const Ent& stomper, const State& state) {
    if (!stomper.active) {
        return false;
    }
    if (!stomper.can_stomp) {
        return false;
    }
    if (stomper.condition != EntCondition::Normal) {
        return false;
    }
    if (stomper.vel.y <= sim::Scalar::zero()) {
        return false;
    }
    if (stomper.held_by_vid.has_value()) {
        return false;
    }
    if (HasMovementFlag(stomper, EntMovementFlag::Hanging)) {
        return false;
    }
    if (GetModifiedEffectValue(stomper, EffectModifierTarget::StompDamageScale, 1.0F, &state) <= 0.0F) {
        return false;
    }
    return true;
}

bool CanEntBeStomped(const Ent& target) {
    if (!target.active) {
        return false;
    }
    if (!target.can_be_stomped) {
        return false;
    }
    if (target.impassable || !target.can_collide) {
        return false;
    }
    if (target.condition != EntCondition::Normal) {
        return false;
    }
    if (HasMovementFlag(target, EntMovementFlag::Hanging)) {
        return false;
    }
    return true;
}

void ApplyStompBounce(Ent& stomper, State& state) {
    const controls::ControlIntent control = controls::GetControlIntentForEnt(stomper, state);
    const float base_bounce_impulse = control.jump
        ? -kStompHeldJumpBounceVelocityY
        : -kStompShortBounceVelocityY;
    const float bounce_impulse =
        GetModifiedEffectValue(stomper, EffectModifierTarget::StompBounceImpulse, base_bounce_impulse, &state);
    stomper.vel.y = -sim::ToSimScalar(bounce_impulse);
}

} // namespace

bool TryApplyStompContactToEnt(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& stomper = state.ents.ents[ent_idx];
    Ent* const stomped = state.ents.GetEntMut(state.ents.ents[other_ent_idx].vid);
    if (stomped == nullptr) {
        return false;
    }

    if (!CanEntAttemptStomp(stomper, state)) {
        return false;
    }
    if (!CanEntBeStomped(*stomped)) {
        return false;
    }
    if (state.contact.HasInteractionCooldown(
            stomper.vid,
            stomped->vid,
            InteractionCooldownKind::Harm
        )) {
        return false;
    }

    const sim::FxAABB stomper_aabb = GetContactAabbForEnt(stomper, graphics);
    const sim::FxAABB stomped_aabb = GetNearestWorldAabb(
        state.stage,
        stomper_aabb.center(),
        GetContactAabbForEnt(*stomped, graphics)
    );
    const sim::Scalar stomped_head_band_bottom =
        stomped_aabb.tl.y + sim::ToSimScalar(kStompHeadHeight);
    if (stomper_aabb.br.y > stomped_head_band_bottom) {
        return false;
    }

    (void)PlayEntCenterSoundEmitter(state, stomper, audio_asset_ids::Jump);
    const bool has_spring_shoes = HasEffect(stomper, EffectId::SpringShoes);
    if (has_spring_shoes) {
        (void)PlayEntCenterSoundEmitter(
            state,
            stomper,
            audio_asset_ids::SpringShoe,
            AudioEmitterPlayParams{.volume_scale = kSpringShoeMovementSoundVolume}
        );
    }

    if (IsPlayerLikeEntType(stomper.type_) && IsPlayerLikeEntType(stomped->type_)) {
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
    const std::uint32_t stomp_damage =
        static_cast<std::uint32_t>(std::max(0.0F, base_stomp_damage * stomp_damage_scale));
    if (stomp_damage == 0) {
        return false;
    }
    const sim::FxVec2 stomp_delta =
        GetNearestWorldDelta(state.stage, stomper.GetSimAABB().center(), stomped->GetSimAABB().center());
    const sim::Scalar stomp_knockback_x =
        stomp_delta.x < sim::Scalar::zero() ? -sim::ToSimScalar(kStompVictimKnockbackVelocityX)
                                            : sim::ToSimScalar(kStompVictimKnockbackVelocityX);
    const KnockbackSpec knockback{
        .velocity = sim::FxVec2{
            stomp_knockback_x,
            sim::ToSimScalar(kStompVictimKnockbackVelocityY),
        },
        .clear_velocity = true,
        .clear_acceleration = true,
        .thrown_by = stomper.vid,
        .thrown_immunity_timer = kThrownByImmunityDuration,
        .proj_contact_damage_type = DamageType::Attack,
        .proj_contact_damage_amount = 1,
        .proj_contact_duration = kProjContactDuration,
    };
    (void)TryHitEnt(
        stomped->vid.id,
        state,
        audio,
        DamageType::JumpOn,
        stomp_damage,
        HitOptions{
            .source_vid = stomper.vid,
            .knockback = knockback,
        }
    );
    if (stomped->can_be_stunned) {
        stomped->condition = EntCondition::Stunned;
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

} // namespace splonks::ents::common

#include "ents/machete.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "ents/common/common.hpp"
#include "ents/sac_altar.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace splonks::ents::machete {

namespace {

constexpr int kMacheteStrikePending = 1;
constexpr std::uint32_t kMacheteDamage = 8;
constexpr std::int32_t kThrownKillFavor = 1;
constexpr std::int32_t kCorpseCutFavor = 1;

bool IsSwinging(const Ent& machete) {
    return machete.aframe_animator.anim_id == aframe_ids::KnifeSwing;
}

std::int32_t GetPendingFavor(const Ent& machete) {
    return std::max(0, machete.counter_b.trunc_int());
}

void AddPendingFavor(Ent& machete, std::int32_t amount) {
    machete.counter_b = sim::Scalar::from_int(GetPendingFavor(machete) + std::max(0, amount));
}

void ClearPendingFavor(Ent& machete) {
    machete.counter_b = sim::Scalar::zero();
}

FVec2 GetVictimEffectPos(const Ent& victim, const Graphics& graphics) {
    const FAABB render_victim_aabb = ToFAABB(common::GetContactAabbForEnt(victim, graphics));
    return FVec2::New(
        (render_victim_aabb.tl.x + render_victim_aabb.br.x) * 0.5F,
        render_victim_aabb.br.y - 2.0F
    );
}

bool CanMacheteHitEnt(const Ent& machete, const Ent* holder, const Ent& other_ent) {
    if (!other_ent.active || !other_ent.can_collide) {
        return false;
    }
    if (other_ent.impassable || other_ent.condition == EntCondition::Dead) {
        return false;
    }
    if (other_ent.vid == machete.vid) {
        return false;
    }
    if (holder != nullptr && other_ent.vid == holder->vid) {
        return false;
    }
    if (holder != nullptr && other_ent.held_by_vid.has_value() && *other_ent.held_by_vid == holder->vid) {
        return false;
    }
    return true;
}

bool CanCarveCorpse(const Ent& victim) {
    return victim.active && victim.condition == EntCondition::Dead &&
           ents::sac_altar::GetSacrificeFavorValue(victim).has_value();
}

void CarveCorpse(Ent& machete, Ent& victim, State& state, const Graphics& graphics, Audio& audio) {
    const FVec2 effect_pos = GetVictimEffectPos(victim, graphics);
    common::DropHeldItemFromEnt(victim, state);
    common::ReleaseEntFromHolder(victim, state);
    victim.marked_for_destruction = true;
    (void)world_ops::DeactivateEnt(state, victim.vid);
    state.UpdateSidForEnt(victim.vid.id, graphics);
    AddPendingFavor(machete, kCorpseCutFavor);
    ents::sac_altar::SpawnSacrificeGainEffects(state, audio, effect_pos);
}

void HandleHeldKillFavor(Ent& machete, const Ent& victim_before_damage, State& state, const Graphics& graphics, Audio& audio) {
    const std::optional<std::int32_t> favor = ents::sac_altar::GetLivingSacrificeFavorValue(victim_before_damage);
    if (!favor.has_value()) {
        return;
    }
    AddPendingFavor(machete, *favor);
    ents::sac_altar::SpawnSacrificeGainEffects(state, audio, GetVictimEffectPos(victim_before_damage, graphics));
}

void HandleThrownKillFavor(Ent& machete, const Ent& victim, State& state, const Graphics& graphics, Audio& audio) {
    AddPendingFavor(machete, kThrownKillFavor);
    ents::sac_altar::SpawnSacrificeGainEffects(state, audio, GetVictimEffectPos(victim, graphics));
}

bool TryDepositFavorWhileGroundedOnSacAltar(
    Ent& machete,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (machete.held_by_vid.has_value() || !machete.grounded || GetPendingFavor(machete) <= 0) {
        return false;
    }

    const sim::AABB feet = machete.GetSimFeet();
    for (const VID& other_vid : QueryEntsInAabb(state, feet, machete.vid)) {
        Ent* const other_ent = state.ents.GetEntMut(other_vid);
        if (other_ent == nullptr || !other_ent->active || other_ent->type_ != EntType::SacAltar) {
            continue;
        }

        const sim::AABB altar_aabb = GetNearestWorldAabb(
            state.stage,
            machete.GetSimCenter(),
            common::GetContactAabbForEnt(*other_ent, graphics)
        );
        if (!gfxp::aabbs_intersect(feet, altar_aabb)) {
            continue;
        }

        if (ents::sac_altar::TryDepositStoredFavor(
                *other_ent,
                GetPendingFavor(machete),
                state,
                graphics,
                audio
            )) {
            ClearPendingFavor(machete);
            return true;
        }
    }

    return false;
}

void TryApplyMacheteStrike(std::size_t ent_idx, State& state, const Graphics& graphics, Audio& audio) {
    Ent& machete = state.ents.ents[ent_idx];
    Ent* holder = nullptr;
    if (machete.held_by_vid.has_value()) {
        holder = state.ents.GetEntMut(*machete.held_by_vid);
    }

    const sim::AABB strike_aabb = common::GetContactAabbForEnt(machete, graphics);
    for (const VID& other_vid : QueryEntsInAabb(state, strike_aabb, machete.vid)) {
        Ent* const other_ent = state.ents.GetEntMut(other_vid);
        if (other_ent == nullptr) {
            continue;
        }

        const sim::AABB other_aabb = GetNearestWorldAabb(
            state.stage,
            strike_aabb.center(),
            common::GetContactAabbForEnt(*other_ent, graphics)
        );
        if (!gfxp::aabbs_intersect(strike_aabb, other_aabb)) {
            continue;
        }

        if (CanCarveCorpse(*other_ent)) {
            CarveCorpse(machete, *other_ent, state, graphics, audio);
            continue;
        }

        if (!CanMacheteHitEnt(machete, holder, *other_ent)) {
            continue;
        }

        const Ent victim_before_damage = *other_ent;
        const sim::Scalar knockback_x =
            sim::ToSimScalar(machete.facing == Side::Left ? -3.5F : 3.5F);
        const common::DamageResult damage_result =
            common::TryHitEnt(
                other_ent->vid.id,
                state,
                audio,
                DamageType::Attack,
                kMacheteDamage,
                common::HitOptions{
                    .source_vid = machete.vid,
                    .knockback = common::KnockbackSpec{
                        .velocity = sim::Vec2{knockback_x, sim::ToSimScalar(-1.5F)},
                        .clear_velocity = true,
                        .clear_acceleration = true,
                        .thrown_by = holder != nullptr ? std::optional<VID>(holder->vid) : std::nullopt,
                        .thrown_immunity_timer = common::kThrownByImmunityDuration,
                        .proj_contact_damage_type = DamageType::Attack,
                        .proj_contact_damage_amount = kMacheteDamage,
                        .proj_contact_duration = common::kProjContactDuration,
                    },
                }
            );
        if (damage_result == common::DamageResult::None) {
            continue;
        }

        if (damage_result == common::DamageResult::Died) {
            HandleHeldKillFavor(machete, victim_before_damage, state, graphics, audio);
        }

    }
}

common::ContactResult OnEntContactAsMachete(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr || context.phase != common::ContactPhase::SweptEntered) {
        return common::ContactResult{};
    }
    if (ent_idx >= state.ents.ents.size() || other_ent_idx >= state.ents.ents.size()) {
        return common::ContactResult{};
    }

    Ent& machete = state.ents.ents[ent_idx];
    Ent& other_ent = state.ents.ents[other_ent_idx];
    if (!machete.active || machete.type_ != EntType::Machete || !other_ent.active) {
        return common::ContactResult{};
    }

    if (machete.proj_contact_timer == 0 || machete.held_by_vid.has_value()) {
        return common::ContactResult{};
    }

    if (other_ent.type_ == EntType::Cobweb) {
        (void)common::TryDamageEnt(other_ent_idx, state, *audio, DamageType::Attack, kMacheteDamage);
        return common::ContactResult{.stop_sweep = true};
    }

    if (CanCarveCorpse(other_ent) && other_ent.last_condition == EntCondition::Dead) {
        CarveCorpse(machete, other_ent, state, *graphics, *audio);
        return common::ContactResult{.stop_sweep = true};
    }

    if (other_ent.condition == EntCondition::Dead && other_ent.last_condition != EntCondition::Dead &&
        ents::sac_altar::GetSacrificeFavorValue(other_ent).has_value()) {
        HandleThrownKillFavor(machete, other_ent, state, *graphics, *audio);
    }

    return common::ContactResult{};
}

} // namespace

void OnUseAsMachete(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)state;
    (void)graphics;
    (void)audio;
    Ent& machete = state.ents.ents[ent_idx];
    if (!machete.use_state.pressed || IsSwinging(machete)) {
        return;
    }

    SetAnim(machete, aframe_ids::KnifeSwing);
    machete.aframe_animator.loop = false;
    machete.counter_a = sim::Scalar::from_int(kMacheteStrikePending);
    (void)PlayEntCenterSoundEmitter(state, machete, audio_asset_ids::Throw);

    if (machete.use_state.source == AttachMode::None) {
        StopUsingEnt(machete);
    }
}

void StepEntLogicAsMachete(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    Ent& machete = state.ents.ents[ent_idx];
    TryDepositFavorWhileGroundedOnSacAltar(machete, state, graphics, audio);
    if (!IsSwinging(machete)) {
        return;
    }

    if (machete.counter_a > sim::Scalar::zero() && machete.aframe_animator.current_frame > 0) {
        TryApplyMacheteStrike(ent_idx, state, graphics, audio);
        machete.counter_a = sim::Scalar::zero();
    }

    if (!machete.aframe_animator.IsFinished()) {
        return;
    }

    SetAnim(machete, aframe_ids::Knife);
    machete.aframe_animator.loop = true;
}

extern const EntSpec kMacheteSpec{
    .type_ = EntType::Machete,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .proj_contact_damage_type = DamageType::Attack,
    .proj_contact_damage_amount = kMacheteDamage,
    .on_use = OnUseAsMachete,
    .step_logic = StepEntLogicAsMachete,
    .on_ent_contact = OnEntContactAsMachete,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Knife),
};

} // namespace splonks::ents::machete

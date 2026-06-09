#include "ents/bow.hpp"

#include "audio.hpp"
#include "effects.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/common/discrete_aim.hpp"
#include "aframe_id.hpp"
#include "fxp.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace splonks::ents::bow {

namespace {

constexpr float kBowFireCooldownFrames = 10.0F;
constexpr float kBowArrowAmmo = 8.0F;
constexpr float kBowArrowSpeed = 8.0F;
constexpr std::uint32_t kBowArrowDamage = 2;

using BowAim = common::DiscreteHeldWeaponAim;

bool HasAmmo(const Ent& bow) {
    return bow.counter_b > FxScalar::zero();
}

bool IsArmed(const Ent& bow) {
    return bow.ent_a.has_value();
}

AFrameId GetLooseAnimId(const Ent& bow) {
    return HasAmmo(bow) ? aframe_ids::BowLooseLoaded : aframe_ids::BowLooseEmpty;
}

AFrameId GetPullAnimId(const Ent& bow) {
    return HasAmmo(bow) ? aframe_ids::BowPullLoaded : aframe_ids::BowPullEmpty;
}

std::string FormatHudInt(int value) {
    char text[16];
    std::snprintf(text, sizeof(text), "%d", value);
    return std::string(text);
}

void BuildHudEntryAsBow(
    const Ent& bow,
    const State& state,
    HudEntrySource source,
    HudEntry& entry
) {
    (void)state;
    (void)source;
    const int ammo = std::max(0, bow.counter_b.trunc_int());
    entry.icon_anim_id = ammo > 0 ? aframe_ids::BowLooseLoaded : aframe_ids::BowLooseEmpty;
    entry.count_text = FormatHudInt(ammo);
    entry.count_anchor = HudAnchor::BottomRight;
    entry.style = ammo > 0 ? HudEntryStyle::Normal : HudEntryStyle::Dimmed;
}

BowAim GetBowAim(const Ent& bow, const State& state) {
    const Ent* const holder =
        bow.held_by_vid.has_value() ? state.ents.GetEnt(*bow.held_by_vid) : nullptr;
    return common::GetDiscreteHeldWeaponAim(bow, holder, state);
}

void ArmBow(Ent& bow, State& state) {
    if (bow.counter_a > FxScalar::zero() || !HasAmmo(bow)) {
        return;
    }

    bow.counter_a = ToFxScalar(kBowFireCooldownFrames);
    bow.ent_a = bow.held_by_vid;
    const BowAim aim = GetBowAim(bow, state);
    bow.facing = aim.facing;
    bow.rotation = aim.rotation;
    bow.aframe_animator.PlayOnce(GetPullAnimId(bow));
    (void)PlayEntCenterSoundEmitter(state, bow, audio_asset_ids::Throw);
}

void SpawnArrowFromBow(Ent& bow, State& state, const BowAim& aim) {
    (void)world_ops::SpawnEnt(state, EntType::Arrow, [&](Ent& arrow) {
        const FxVec2 spawn_center = bow.GetCenter() +
                                       (aim.direction * FxScalar::from_int(12));
        arrow.SetCenter(FxVec2::from_int(spawn_center.x.round_int(),
                                          spawn_center.y.round_int()));
        arrow.vel = aim.direction * ToFxScalar(kBowArrowSpeed);
        arrow.acc = FxVec2::zero();
        arrow.facing = aim.facing;
        arrow.rotation = aim.rotation;
        arrow.thrown_by = bow.ent_a.has_value() ? bow.ent_a : bow.held_by_vid;
        arrow.thrown_immunity_timer = ents::common::kThrownByImmunityDuration;
        arrow.proj_contact_damage_type = DamageType::Attack;
        arrow.proj_contact_damage_amount = kBowArrowDamage;
        arrow.proj_contact_timer = ents::common::kProjContactDuration;
        arrow.can_apply_proj_contact = false;
        (void)AddEffect(arrow, EffectId::NoGravityUntilContact);
    });
}

void FireBow(Ent& bow, State& state) {
    if (!HasAmmo(bow)) {
        (void)PlayEntSoundEmitter(state, bow, audio_asset_ids::GunEmpty);
        return;
    }

    const BowAim aim = GetBowAim(bow, state);
    bow.facing = aim.facing;
    bow.rotation = aim.rotation;
    SpawnArrowFromBow(bow, state, aim);
    bow.counter_b -= FxScalar::from_int(1);
    bow.ent_a.reset();
    SetAnim(bow, GetLooseAnimId(bow));
    (void)PlayWorldSoundEmitter(state, ToFVec2(bow.GetCenter()), audio_asset_ids::Throw);
}

} // namespace

void OnUseAsBow(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& bow = state.ents.ents[ent_idx];
    if (bow.use_state.pressed) {
        if (!HasAmmo(bow)) {
            (void)PlayEntSoundEmitter(state, bow, audio_asset_ids::GunEmpty);
            return;
        }
        ArmBow(bow, state);
        return;
    }

    if (bow.use_state.released && bow.ent_a.has_value()) {
        FireBow(bow, state);
    }
}

void StepEntLogicAsBow(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& bow = state.ents.ents[ent_idx];
    if (bow.counter_a > FxScalar::zero()) {
        bow.counter_a =
            gfxp::max(FxScalar::zero(), bow.counter_a - FxScalar::from_int(1));
    }
    if (bow.held_by_vid.has_value()) {
        const BowAim aim = GetBowAim(bow, state);
        bow.facing = aim.facing;
        bow.rotation = aim.rotation;
    }
    if (!IsArmed(bow)) {
        SetAnim(bow, GetLooseAnimId(bow));
    }
}

extern const EntSpec kBowSpec{
    .type_ = EntType::Bow,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .preserve_held_aim = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_b = ToFxScalar(kBowArrowAmmo),
    .damage_vuln = DamageVuln::Vulnerable,
    .on_use = OnUseAsBow,
    .step_logic = StepEntLogicAsBow,
    .build_hud_entry = BuildHudEntryAsBow,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Bow),
};

} // namespace splonks::ents::bow

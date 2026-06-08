#include "ents/bow.hpp"

#include "audio.hpp"
#include "controls.hpp"
#include "effects.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "sim/fxp.hpp"
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
constexpr float kDiagonalAimComponent = 0.707106769F;

struct BowAim {
    Vec2 direction = Vec2::New(1.0F, 0.0F);
    Side facing = Side::Right;
    sim::Scalar rotation = sim::Scalar::zero();
};

bool HasAmmo(const Ent& bow) {
    return bow.counter_b > 0.0F;
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
    const int ammo = static_cast<int>(std::max(0.0F, bow.counter_b));
    entry.icon_anim_id = ammo > 0 ? aframe_ids::BowLooseLoaded : aframe_ids::BowLooseEmpty;
    entry.count_text = FormatHudInt(ammo);
    entry.count_anchor = HudAnchor::BottomRight;
    entry.style = ammo > 0 ? HudEntryStyle::Normal : HudEntryStyle::Dimmed;
}

float NormalizeDegrees(float degrees) {
    while (degrees > 180.0F) {
        degrees -= 360.0F;
    }
    while (degrees <= -180.0F) {
        degrees += 360.0F;
    }
    return degrees;
}

Vec2 DiscreteAimDirection(int aim_x, int aim_y, Side facing) {
    if (aim_x == 0 && aim_y == 0) {
        return facing == Side::Left ? Vec2::New(-1.0F, 0.0F) : Vec2::New(1.0F, 0.0F);
    }
    if (aim_x != 0 && aim_y != 0) {
        return Vec2::New(
            static_cast<float>(aim_x) * kDiagonalAimComponent,
            static_cast<float>(aim_y) * kDiagonalAimComponent
        );
    }
    return Vec2::New(static_cast<float>(aim_x), static_cast<float>(aim_y));
}

float DiscreteAimWorldAngle(int aim_x, int aim_y, Side facing) {
    if (aim_x == 0 && aim_y == 0) {
        return facing == Side::Left ? 180.0F : 0.0F;
    }
    if (aim_x > 0) {
        if (aim_y < 0) {
            return -45.0F;
        }
        if (aim_y > 0) {
            return 45.0F;
        }
        return 0.0F;
    }
    if (aim_x < 0) {
        if (aim_y < 0) {
            return -135.0F;
        }
        if (aim_y > 0) {
            return 135.0F;
        }
        return 180.0F;
    }
    return aim_y < 0 ? -90.0F : 90.0F;
}

BowAim GetBowAim(const Ent& bow, const State& state) {
    const Ent* const holder =
        bow.held_by_vid.has_value() ? state.ents.GetEnt(*bow.held_by_vid) : nullptr;

    int aim_x = 0;
    int aim_y = 0;
    Side facing = holder != nullptr ? holder->facing : bow.facing;
    if (holder != nullptr) {
        const controls::ControlIntent intent = controls::GetControlIntentForEnt(*holder, state);
        if (intent.left && !intent.right) {
            aim_x = -1;
        } else if (intent.right && !intent.left) {
            aim_x = 1;
        }
        if (intent.up && !intent.down) {
            aim_y = -1;
        } else if (intent.down && !intent.up) {
            aim_y = 1;
        }
    }

    if (aim_x < 0) {
        facing = Side::Left;
    } else if (aim_x > 0) {
        facing = Side::Right;
    }

    const Vec2 direction = DiscreteAimDirection(aim_x, aim_y, facing);
    const float world_angle = DiscreteAimWorldAngle(aim_x, aim_y, facing);
    const float base_angle = facing == Side::Left ? 180.0F : 0.0F;
    return BowAim{
        .direction = direction,
        .facing = facing,
        .rotation = sim::ToSimScalar(NormalizeDegrees(world_angle - base_angle)),
    };
}

void ArmBow(Ent& bow, State& state) {
    if (bow.counter_a > 0.0F || !HasAmmo(bow)) {
        return;
    }

    bow.counter_a = kBowFireCooldownFrames;
    bow.ent_a = bow.held_by_vid;
    const BowAim aim = GetBowAim(bow, state);
    bow.facing = aim.facing;
    bow.rotation = aim.rotation;
    bow.aframe_animator.PlayOnce(GetPullAnimId(bow));
    (void)PlayEntCenterSoundEmitter(state, bow, audio_asset_ids::Throw);
}

void SpawnArrowFromBow(Ent& bow, State& state, const BowAim& aim) {
    (void)world_ops::SpawnEnt(state, EntType::Arrow, [&](Ent& arrow) {
        const Vec2 direction = aim.direction;
        const Vec2 spawn_center = bow.GetCenter() + direction * 12.0F;
        arrow.SetCenter(Vec2::New(static_cast<float>(RoundToInt(spawn_center.x)),
                                  static_cast<float>(RoundToInt(spawn_center.y))));
        arrow.vel = sim::ToSimVec2(direction * kBowArrowSpeed);
        arrow.acc = sim::Vec2::zero();
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
    bow.counter_b -= 1.0F;
    bow.ent_a.reset();
    SetAnim(bow, GetLooseAnimId(bow));
    (void)PlayWorldSoundEmitter(state, bow.GetCenter(), audio_asset_ids::Throw);
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
    if (bow.counter_a > 0.0F) {
        bow.counter_a = std::max(0.0F, bow.counter_a - 1.0F);
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
    .counter_b = EntSpecCounter(kBowArrowAmmo),
    .damage_vuln = DamageVuln::Vulnerable,
    .on_use = OnUseAsBow,
    .step_logic = StepEntLogicAsBow,
    .build_hud_entry = BuildHudEntryAsBow,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Bow),
};

} // namespace splonks::ents::bow

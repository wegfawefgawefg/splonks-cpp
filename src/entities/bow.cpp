#include "entities/bow.hpp"

#include "audio.hpp"
#include "controls.hpp"
#include "effects.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace splonks::entities::bow {

namespace {

constexpr float kBowFireCooldownFrames = 10.0F;
constexpr float kBowArrowAmmo = 8.0F;
constexpr float kBowArrowSpeed = 8.0F;
constexpr unsigned int kBowArrowDamage = 2;

struct BowAim {
    Vec2 direction = Vec2::New(1.0F, 0.0F);
    LeftOrRight facing = LeftOrRight::Right;
    float rotation = 0.0F;
};

bool HasAmmo(const Entity& bow) {
    return bow.counter_b > 0.0F;
}

bool IsArmed(const Entity& bow) {
    return bow.entity_a.has_value();
}

FrameDataId GetLooseAnimationId(const Entity& bow) {
    return HasAmmo(bow) ? frame_data_ids::BowLooseLoaded : frame_data_ids::BowLooseEmpty;
}

FrameDataId GetPullAnimationId(const Entity& bow) {
    return HasAmmo(bow) ? frame_data_ids::BowPullLoaded : frame_data_ids::BowPullEmpty;
}

std::string FormatHudInt(int value) {
    char text[16];
    std::snprintf(text, sizeof(text), "%d", value);
    return std::string(text);
}

void BuildHudEntryAsBow(
    const Entity& bow,
    const State& state,
    HudEntrySource source,
    HudEntry& entry
) {
    (void)state;
    (void)source;
    const int ammo = static_cast<int>(std::max(0.0F, bow.counter_b));
    entry.icon_animation_id = ammo > 0 ? frame_data_ids::BowLooseLoaded : frame_data_ids::BowLooseEmpty;
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

BowAim GetBowAim(const Entity& bow, const State& state) {
    const Entity* const holder =
        bow.held_by_vid.has_value() ? state.entity_manager.GetEntity(*bow.held_by_vid) : nullptr;

    int aim_x = 0;
    int aim_y = 0;
    LeftOrRight facing = holder != nullptr ? holder->facing : bow.facing;
    if (holder != nullptr) {
        const controls::ControlIntent intent = controls::GetControlIntentForEntity(*holder, state);
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
        facing = LeftOrRight::Left;
    } else if (aim_x > 0) {
        facing = LeftOrRight::Right;
    }

    Vec2 direction = Vec2::New(static_cast<float>(aim_x), static_cast<float>(aim_y));
    if (direction == Vec2::New(0.0F, 0.0F)) {
        direction = facing == LeftOrRight::Left ? Vec2::New(-1.0F, 0.0F) : Vec2::New(1.0F, 0.0F);
    } else {
        direction = NormalizeOrZero(direction);
    }

    const float world_angle = std::atan2(direction.y, direction.x) * (180.0F / 3.14159265F);
    const float base_angle = facing == LeftOrRight::Left ? 180.0F : 0.0F;
    return BowAim{
        .direction = direction,
        .facing = facing,
        .rotation = NormalizeDegrees(world_angle - base_angle),
    };
}

void ArmBow(Entity& bow, State& state) {
    if (bow.counter_a > 0.0F || !HasAmmo(bow)) {
        return;
    }

    bow.counter_a = kBowFireCooldownFrames;
    bow.entity_a = bow.held_by_vid;
    const BowAim aim = GetBowAim(bow, state);
    bow.facing = aim.facing;
    bow.rotation = aim.rotation;
    bow.frame_data_animator.PlayOnce(GetPullAnimationId(bow));
    (void)PlayEntityCenterSoundEmitter(state, bow, audio_asset_ids::Throw);
}

void SpawnArrowFromBow(Entity& bow, State& state, const BowAim& aim) {
    (void)world_ops::SpawnEntity(state, EntityType::Arrow, [&](Entity& arrow) {
        const Vec2 direction = aim.direction;
        const Vec2 spawn_center = bow.GetCenter() + direction * 12.0F;
        arrow.SetCenter(Vec2::New(std::round(spawn_center.x), std::round(spawn_center.y)));
        arrow.vel = direction * kBowArrowSpeed;
        arrow.acc = Vec2::New(0.0F, 0.0F);
        arrow.facing = aim.facing;
        arrow.rotation = aim.rotation;
        arrow.thrown_by = bow.entity_a.has_value() ? bow.entity_a : bow.held_by_vid;
        arrow.thrown_immunity_timer = entities::common::kThrownByImmunityDuration;
        arrow.projectile_contact_damage_type = DamageType::Attack;
        arrow.projectile_contact_damage_amount = kBowArrowDamage;
        arrow.projectile_contact_timer = entities::common::kProjectileContactDuration;
        arrow.can_apply_projectile_contact = false;
        (void)AddEffect(arrow, EffectId::NoGravityUntilContact);
    });
}

void FireBow(Entity& bow, State& state) {
    if (!HasAmmo(bow)) {
        (void)PlayEntitySoundEmitter(state, bow, audio_asset_ids::GunEmpty);
        return;
    }

    const BowAim aim = GetBowAim(bow, state);
    bow.facing = aim.facing;
    bow.rotation = aim.rotation;
    SpawnArrowFromBow(bow, state, aim);
    bow.counter_b -= 1.0F;
    bow.entity_a.reset();
    SetAnimation(bow, GetLooseAnimationId(bow));
    (void)PlayWorldSoundEmitter(state, bow.GetCenter(), audio_asset_ids::Throw);
    world_ops::PatchEntityState(state, bow, bow);
}

} // namespace

void OnUseAsBow(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& bow = state.entity_manager.entities[entity_idx];
    if (bow.use_state.pressed) {
        if (!HasAmmo(bow)) {
            (void)PlayEntitySoundEmitter(state, bow, audio_asset_ids::GunEmpty);
            return;
        }
        ArmBow(bow, state);
        return;
    }

    if (bow.use_state.released && bow.entity_a.has_value()) {
        FireBow(bow, state);
    }
}

void StepEntityLogicAsBow(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& bow = state.entity_manager.entities[entity_idx];
    if (bow.counter_a > 0.0F) {
        bow.counter_a = std::max(0.0F, bow.counter_a - 1.0F);
    }
    if (bow.held_by_vid.has_value()) {
        const BowAim aim = GetBowAim(bow, state);
        bow.facing = aim.facing;
        bow.rotation = aim.rotation;
    }
    if (!IsArmed(bow)) {
        SetAnimation(bow, GetLooseAnimationId(bow));
    }
}

extern const EntityArchetype kBowArchetype{
    .type_ = EntityType::Bow,
    .size = Vec2::New(16.0F, 16.0F),
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .counter_b = kBowArrowAmmo,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_use = OnUseAsBow,
    .step_logic = StepEntityLogicAsBow,
    .build_hud_entry = BuildHudEntryAsBow,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Bow),
};

} // namespace splonks::entities::bow

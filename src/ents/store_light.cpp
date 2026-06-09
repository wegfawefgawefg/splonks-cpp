#include "ents/store_light.hpp"

#include "audio.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"

namespace splonks::ents::store_light {

namespace {

constexpr float kStoreLightStrength = 1.70F;
constexpr Color3 kStoreLightColor = Color3::White();
constexpr int kStoreLightRadius = kStoreLightRadiusTiles;

bool IsStoreLightBroken(const Ent& ent) {
    return ent.has_physics;
}

} // namespace

void AttachStoreLight(Ent& ent, State& state, int radius) {
    (void)state;
    ent.light_strength = ToFxScalar(kStoreLightStrength);
    ent.light_color = ToFxColor3(kStoreLightColor);
    ent.light_radius = radius;
}

EntDamageEffectResult OnDamageAsStoreLight(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    (void)audio;
    (void)amount;
    (void)damage_applied;

    if (damage_type == DamageType::JumpOn || ent_idx >= state.ents.ents.size()) {
        return EntDamageEffectResult::None;
    }

    Ent& light = state.ents.ents[ent_idx];
    if (!light.active || IsStoreLightBroken(light)) {
        return EntDamageEffectResult::None;
    }

    SetAnim(light, aframe_ids::StoreLightBroken);
    light.has_physics = true;
    light.can_collide = true;
    light.damage_vuln = DamageVuln::Immune;
    light.collide_sound = audio_asset_ids::LightBreak;
    light.light_strength = sim::Scalar::zero();
    light.light_radius = 0;
    (void)PlayEntCenterSoundEmitter(state, light, audio_asset_ids::LightBreak);
    return EntDamageEffectResult::Consumed;
}

extern const EntSpec kStoreLightSpec{
    .type_ = EntType::StoreLight,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_hit = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .light_strength = ToFxScalar(kStoreLightStrength),
    .light_color = ToFxColor3(kStoreLightColor),
    .light_radius = kStoreLightRadius,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::AnthingExceptJumpOn,
    .damage_sound = audio_asset_ids::LightBreak,
    .collide_sound = audio_asset_ids::LightBreak,
    .on_damage = OnDamageAsStoreLight,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::StoreLight),
};

} // namespace splonks::ents::store_light

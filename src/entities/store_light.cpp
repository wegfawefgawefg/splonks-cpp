#include "entities/store_light.hpp"

#include "audio.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "state.hpp"

namespace splonks::entities::store_light {

namespace {

constexpr float kStoreLightStrength = 1.70F;
constexpr Color3 kStoreLightColor = Color3::White();
constexpr int kStoreLightRadius = kStoreLightRadiusTiles;

bool IsStoreLightBroken(const Entity& entity) {
    return entity.has_physics;
}

} // namespace

void AttachStoreLight(Entity& entity, State& state, int radius) {
    (void)state;
    entity.light_strength = kStoreLightStrength;
    entity.light_color = kStoreLightColor;
    entity.light_radius = radius;
}

EntityDamageEffectResult OnDamageAsStoreLight(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    (void)audio;
    (void)amount;
    (void)damage_applied;

    if (damage_type == DamageType::JumpOn || entity_idx >= state.entity_manager.entities.size()) {
        return EntityDamageEffectResult::None;
    }

    Entity& light = state.entity_manager.entities[entity_idx];
    if (!light.active || IsStoreLightBroken(light)) {
        return EntityDamageEffectResult::None;
    }

    SetAnimation(light, frame_data_ids::StoreLightBroken);
    light.has_physics = true;
    light.can_collide = true;
    light.damage_vulnerability = DamageVulnerability::Immune;
    light.collide_sound = audio_asset_ids::LightBreak;
    light.light_strength = 0.0F;
    light.light_radius = 0;
    (void)PlayEntityCenterSoundEmitter(state, light, audio_asset_ids::LightBreak);
    return EntityDamageEffectResult::Consumed;
}

extern const EntityArchetype kStoreLightArchetype{
    .type_ = EntityType::StoreLight,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_hit = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .light_strength = kStoreLightStrength,
    .light_color = kStoreLightColor,
    .light_radius = kStoreLightRadius,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .damage_sound = audio_asset_ids::LightBreak,
    .collide_sound = audio_asset_ids::LightBreak,
    .on_damage = OnDamageAsStoreLight,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::StoreLight),
};

} // namespace splonks::entities::store_light

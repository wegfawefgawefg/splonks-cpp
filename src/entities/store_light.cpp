#include "entities/store_light.hpp"

#include "audio.hpp"
#include "frame_data_id.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"

namespace splonks::entities::store_light {

namespace {

IVec2 GetStoreLightTilePos(const Entity& entity, const State& state) {
    return state.stage.GetTileCoordAtWc(ToIVec2(entity.pos));
}

bool IsStoreLightBroken(const Entity& entity) {
    return entity.has_physics;
}

} // namespace

void AttachStoreLight(Entity& entity, State& state, int radius) {
    entity.entity_a = AddStageLight(state, GetStoreLightTilePos(entity, state), radius);
}

EntityDamageEffectResult OnDamageAsStoreLight(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    (void)amount;
    (void)damage_applied;

    if (damage_type == DamageType::JumpOn || entity_idx >= state.entity_manager.entities.size()) {
        return EntityDamageEffectResult::None;
    }

    Entity& light = state.entity_manager.entities[entity_idx];
    if (!light.active || IsStoreLightBroken(light)) {
        return EntityDamageEffectResult::None;
    }

    if (light.entity_a.has_value()) {
        (void)RemoveStageLight(state, *light.entity_a);
        light.entity_a.reset();
    }

    SetAnimation(light, frame_data_ids::StoreLightBroken);
    light.has_physics = true;
    light.can_collide = true;
    light.damage_vulnerability = DamageVulnerability::Immune;
    light.collide_sound = SoundEffect::LightBreak;
    audio.PlaySoundEffect(SoundEffect::LightBreak);
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
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .damage_sound = SoundEffect::LightBreak,
    .collide_sound = SoundEffect::LightBreak,
    .on_damage = OnDamageAsStoreLight,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::StoreLight),
};

} // namespace splonks::entities::store_light

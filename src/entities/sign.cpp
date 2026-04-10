#include "entities/sign.hpp"

#include "frame_data_id.hpp"

namespace splonks::entities::sign {

namespace {

void SetEntitySign(Entity& entity, EntityType type_, FrameDataId animation_id) {
    entity.Reset();
    entity.type_ = type_;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    TrySetDisplayState(entity, EntityDisplayState::Neutral);
    entity.size = Vec2::New(16.0F, 16.0F);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.has_physics = false;
    entity.can_collide = false;
    entity.can_be_picked_up = false;
    entity.impassable = false;
    entity.hurt_on_contact = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Middle;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    entity.frame_data_animator.SetAnimation(animation_id);
}

} // namespace

void SetEntitySignGeneral(Entity& entity) {
    SetEntitySign(entity, EntityType::SignGeneral, frame_data_ids::SignGeneral);
}

void SetEntitySignBomb(Entity& entity) {
    SetEntitySign(entity, EntityType::SignBomb, frame_data_ids::SignBomb);
}

void SetEntitySignWeapon(Entity& entity) {
    SetEntitySign(entity, EntityType::SignWeapon, frame_data_ids::SignWeapon);
}

void SetEntitySignRare(Entity& entity) {
    SetEntitySign(entity, EntityType::SignRare, frame_data_ids::SignRare);
}

void SetEntitySignClothing(Entity& entity) {
    SetEntitySign(entity, EntityType::SignClothing, frame_data_ids::SignClothing);
}

void SetEntitySignCraps(Entity& entity) {
    SetEntitySign(entity, EntityType::SignCraps, frame_data_ids::SignCraps);
}

void SetEntitySignKissing(Entity& entity) {
    SetEntitySign(entity, EntityType::SignKissing, frame_data_ids::SignKissing);
}

void StepEntityLogicAsSign(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

} // namespace splonks::entities::sign

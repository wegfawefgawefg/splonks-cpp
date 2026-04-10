#include "entities/altar.hpp"

#include "frame_data_id.hpp"

namespace splonks::entities::altar {

namespace {

void SetEntityAltar(Entity& entity, EntityType type_, FrameDataId animation_id) {
    entity.Reset();
    entity.type_ = type_;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    TrySetDisplayState(entity, EntityDisplayState::Neutral);
    entity.size = Vec2::New(16.0F, 16.0F);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.has_physics = false;
    entity.can_collide = true;
    entity.can_be_picked_up = false;
    entity.impassable = true;
    entity.hurt_on_contact = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Middle;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    entity.frame_data_animator.SetAnimation(animation_id);
}

} // namespace

void SetEntityAltarLeft(Entity& entity) {
    SetEntityAltar(entity, EntityType::AltarLeft, HashFrameDataIdConstexpr("altar_left"));
}

void SetEntityAltarRight(Entity& entity) {
    SetEntityAltar(entity, EntityType::AltarRight, HashFrameDataIdConstexpr("altar_right"));
}

void StepEntityLogicAsAltar(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

} // namespace splonks::entities::altar

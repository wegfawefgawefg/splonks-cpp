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
    SetEntitySign(entity, EntityType::SignGeneral, HashFrameDataIdConstexpr("sign_general"));
}

void SetEntitySignBomb(Entity& entity) {
    SetEntitySign(entity, EntityType::SignBomb, HashFrameDataIdConstexpr("sign_bomb"));
}

void SetEntitySignWeapon(Entity& entity) {
    SetEntitySign(entity, EntityType::SignWeapon, HashFrameDataIdConstexpr("sign_weapon"));
}

void SetEntitySignRare(Entity& entity) {
    SetEntitySign(entity, EntityType::SignRare, HashFrameDataIdConstexpr("sign_rare"));
}

void SetEntitySignClothing(Entity& entity) {
    SetEntitySign(entity, EntityType::SignClothing, HashFrameDataIdConstexpr("sign_clothing"));
}

void SetEntitySignCraps(Entity& entity) {
    SetEntitySign(entity, EntityType::SignCraps, HashFrameDataIdConstexpr("sign_craps"));
}

void SetEntitySignKissing(Entity& entity) {
    SetEntitySign(entity, EntityType::SignKissing, HashFrameDataIdConstexpr("sign_kissing"));
}

void StepEntityLogicAsSign(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

} // namespace splonks::entities::sign

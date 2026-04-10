#include "entities/gear_items.hpp"

namespace splonks::entities::gear_items {

namespace {

void SetCommonGearItem(Entity& entity, EntityType type_) {
    entity.Reset();
    entity.type_ = type_;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    entity.size = Vec2::New(16.0F, 16.0F);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::Vulnerable;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_picked_up = true;
    entity.impassable = false;
    entity.hurt_on_contact = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    entity.frame_data_animator.SetAnimation(GetDefaultAnimationIdForEntityType(type_));
}

} // namespace

void SetEntityCape(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Cape);
}

void SetEntityShotgun(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Shotgun);
}

void SetEntityTeleporter(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Teleporter);
}

void SetEntityGloves(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Gloves);
}

void SetEntitySpectacles(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Spectacles);
}

void SetEntityWebCannon(Entity& entity) {
    SetCommonGearItem(entity, EntityType::WebCannon);
}

void SetEntityPistol(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Pistol);
}

void SetEntityMitt(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Mitt);
}

void SetEntityPaste(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Paste);
}

void SetEntitySpringShoes(Entity& entity) {
    SetCommonGearItem(entity, EntityType::SpringShoes);
}

void SetEntitySpikeShoes(Entity& entity) {
    SetCommonGearItem(entity, EntityType::SpikeShoes);
}

void SetEntityMachete(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Machete);
}

void SetEntityBombBox(Entity& entity) {
    SetCommonGearItem(entity, EntityType::BombBox);
}

void SetEntityBombBag(Entity& entity) {
    SetCommonGearItem(entity, EntityType::BombBag);
}

void SetEntityBow(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Bow);
}

void SetEntityCompass(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Compass);
}

void SetEntityParachute(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Parachute);
}

void SetEntityRopePile(Entity& entity) {
    SetCommonGearItem(entity, EntityType::RopePile);
}

void StepEntityLogicAsGearItem(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

void StepEntityPhysicsAsGearItem(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::ApplyGroundFriction(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::gear_items

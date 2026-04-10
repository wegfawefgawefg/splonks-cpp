#include "entities/gear_items.hpp"

#include "frame_data_id.hpp"

namespace splonks::entities::gear_items {

namespace {

void SetCommonGearItem(Entity& entity, EntityType type_, FrameDataId animation_id) {
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
    entity.frame_data_animator.SetAnimation(animation_id);
}

} // namespace

void SetEntityCape(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Cape, HashFrameDataIdConstexpr("cape_pickup"));
}

void SetEntityShotgun(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Shotgun, HashFrameDataIdConstexpr("shotgun"));
}

void SetEntityTeleporter(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Teleporter, HashFrameDataIdConstexpr("teleporter"));
}

void SetEntityGloves(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Gloves, HashFrameDataIdConstexpr("gloves"));
}

void SetEntitySpectacles(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Spectacles, HashFrameDataIdConstexpr("spectacles"));
}

void SetEntityWebCannon(Entity& entity) {
    SetCommonGearItem(entity, EntityType::WebCannon, HashFrameDataIdConstexpr("web_cannon"));
}

void SetEntityPistol(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Pistol, HashFrameDataIdConstexpr("pistol"));
}

void SetEntityMitt(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Mitt, HashFrameDataIdConstexpr("mitt"));
}

void SetEntityPaste(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Paste, HashFrameDataIdConstexpr("paste"));
}

void SetEntitySpringShoes(Entity& entity) {
    SetCommonGearItem(entity, EntityType::SpringShoes, HashFrameDataIdConstexpr("spring_shoes"));
}

void SetEntitySpikeShoes(Entity& entity) {
    SetCommonGearItem(entity, EntityType::SpikeShoes, HashFrameDataIdConstexpr("spike_shoes"));
}

void SetEntityMachete(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Machete, HashFrameDataIdConstexpr("machete"));
}

void SetEntityBombBox(Entity& entity) {
    SetCommonGearItem(entity, EntityType::BombBox, HashFrameDataIdConstexpr("bomb_box"));
}

void SetEntityBow(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Bow, HashFrameDataIdConstexpr("bow"));
}

void SetEntityCompass(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Compass, HashFrameDataIdConstexpr("compass"));
}

void SetEntityParachute(Entity& entity) {
    SetCommonGearItem(entity, EntityType::Parachute, HashFrameDataIdConstexpr("parachute"));
}

void SetEntityRopePile(Entity& entity) {
    SetCommonGearItem(entity, EntityType::RopePile, HashFrameDataIdConstexpr("rope_pile"));
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

#include "entities/spider_hang.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::spider_hang {

namespace {

void SetEntitySpider(Entity& entity, EntityType type_, FrameDataId animation_id) {
    entity.Reset();
    entity.type_ = type_;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    TrySetDisplayState(entity, EntityDisplayState::Neutral);
    entity.size = Vec2::New(16.0F, 16.0F);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::Vulnerable;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_picked_up = false;
    entity.impassable = false;
    entity.hurt_on_contact = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = true;
    entity.alignment = Alignment::Enemy;
    entity.frame_data_animator.SetAnimation(animation_id);
}

} // namespace

void SetEntitySpiderHang(Entity& entity) {
    SetEntitySpider(entity, EntityType::SpiderHang, HashFrameDataIdConstexpr("spider_hang"));
}

void SetEntityGiantSpiderHang(Entity& entity) {
    SetEntitySpider(
        entity,
        EntityType::GiantSpiderHang,
        HashFrameDataIdConstexpr("giant_spider_hang")
    );
}

void StepEntityLogicAsSpiderHang(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

void StepEntityPhysicsAsSpiderHang(
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

} // namespace splonks::entities::spider_hang

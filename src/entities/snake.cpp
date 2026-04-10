#include "entities/snake.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::snake {

void SetEntitySnake(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::Snake;
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
    entity.frame_data_animator.SetAnimation(HashFrameDataIdConstexpr("snake"));
}

void StepEntityLogicAsSnake(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

void StepEntityPhysicsAsSnake(
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

} // namespace splonks::entities::snake

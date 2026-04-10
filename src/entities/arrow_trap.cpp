#include "entities/arrow_trap.hpp"

#include "frame_data_id.hpp"

namespace splonks::entities::arrow_trap {

void SetEntityArrowTrap(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::ArrowTrap;
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
    entity.alignment = Alignment::Enemy;
    entity.frame_data_animator.SetAnimation(HashFrameDataIdConstexpr("arrow_trap"));
}

void StepEntityLogicAsArrowTrap(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

} // namespace splonks::entities::arrow_trap

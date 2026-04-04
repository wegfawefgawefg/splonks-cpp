#include "entities/money.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "sprite.hpp"
#include "state.hpp"

namespace splonks::entities::money {

void SetEntityMoney(Entity& entity, EntityType type_) {
    entity.Reset();
    entity.type_ = type_;
    entity.super_state = EntitySuperState::Idle;
    entity.display_state = EntityDisplayState::Neutral;
    entity.size = GetSize(type_);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::CrushingOnly;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.impassable = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    switch (type_) {
    case EntityType::Gold:
        entity.sprite_animator.SetSprite(Sprite::Gold);
        break;
    case EntityType::GoldStack:
        entity.sprite_animator.SetSprite(Sprite::GoldStack);
        break;
    default:
        entity.sprite_animator.SetSprite(Sprite::NoSprite);
        break;
    }
}

void StepEntityLogicAsMoney(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsMoney(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, audio);
    common::ApplyGroundFriction(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

Vec2 GetSize(EntityType type_) {
    switch (type_) {
    case EntityType::Gold:
        return Vec2::New(5.0F, 5.0F);
    case EntityType::GoldStack:
        return Vec2::New(10.0F, 6.0F);
    default:
        return Vec2::New(6.0F, 6.0F);
    }
}

} // namespace splonks::entities::money

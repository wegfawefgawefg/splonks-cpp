#include "entities/breakaway_container.hpp"

#include "audio.hpp"
#include "entities/bat.hpp"
#include "entities/common.hpp"
#include "entities/money.hpp"
#include "sprite.hpp"
#include "state.hpp"

#include <random>

namespace splonks::entities::breakaway_container {

namespace {

unsigned int RandomPercent() {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<unsigned int> distribution(0, 99);
    return distribution(generator);
}

} // namespace

void SetEntityBreakawayContainer(Entity& entity, EntityType type_) {
    entity.Reset();
    entity.health = 1;
    entity.active = true;
    entity.type_ = type_;
    entity.size = GetBreakawayContainerSize(type_);
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_picked_up = true;
    entity.damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn;
    entity.hurt_on_contact = false;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Projectile;
    entity.display_state = EntityDisplayState::Neutral;
    entity.facing = LeftOrRight::Left;
    entity.impassable = false;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    switch (type_) {
    case EntityType::Pot:
        entity.sprite_animator.SetSprite(Sprite::Pot);
        break;
    case EntityType::Box:
        entity.sprite_animator.SetSprite(Sprite::Box);
        break;
    default:
        entity.sprite_animator.SetSprite(Sprite::NoSprite);
        break;
    }
}

void StepEntityLogicAsBreakawayContainer(
    std::size_t entity_idx,
    State& state,
    Audio& audio
) {
    (void)audio;
    const Entity& breakaway_container = state.entity_manager.entities[entity_idx];
    const EntitySuperState breakaway_container_super_state = breakaway_container.super_state;
    const Vec2 breakaway_container_pos = breakaway_container.pos;
    const EntityType breakaway_container_type = breakaway_container.type_;
    if (breakaway_container_super_state == EntitySuperState::Dead) {
        const unsigned int random_number = RandomPercent();
        switch (breakaway_container_type) {
        case EntityType::Pot:
            if (random_number <= 10) {
                // spawn a bat
                if (const std::optional<VID> bat_vid = state.entity_manager.NewEntity()) {
                    if (Entity* const bat = state.entity_manager.GetEntityMut(*bat_vid)) {
                        bat::SetEntityBat(*bat);
                        bat->pos = breakaway_container_pos;
                    }
                }
            } else if (random_number <= 40) {
                // spawn a gold
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const new_entity = state.entity_manager.GetEntityMut(*vid)) {
                        money::SetEntityMoney(*new_entity, EntityType::Gold);
                        new_entity->pos = breakaway_container_pos;
                    }
                }
            }
            break;
        case EntityType::Box:
            if (random_number <= 30) {
                // spawn a big gold
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const new_entity = state.entity_manager.GetEntityMut(*vid)) {
                        money::SetEntityMoney(*new_entity, EntityType::GoldStack);
                        new_entity->pos = breakaway_container_pos;
                    }
                }
            } else if (random_number <= 90) {
                // spawn a gold
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const new_entity = state.entity_manager.GetEntityMut(*vid)) {
                        money::SetEntityMoney(*new_entity, EntityType::Gold);
                        new_entity->pos = breakaway_container_pos;
                    }
                }
            }
            break;
        default:
            break;
        }

        return;
    }

    // TODO: reenable pot and box shattering on throw, fall
    //TODO: if you hit the ground, do a clunky sound
    // if you hit something, do breakaway_container damage and try to stun probs
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsBreakawayContainer(
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

Vec2 GetBreakawayContainerSize(EntityType type_) {
    switch (type_) {
    case EntityType::Pot:
        return Vec2::New(8.0F, 7.0F);
    case EntityType::Box:
        return Vec2::New(12.0F, 12.0F);
    default:
        return Vec2::New(1.0F, 1.0F);
    }
}

} // namespace splonks::entities::breakaway_container

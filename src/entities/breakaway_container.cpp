#include "entities/breakaway_container.hpp"

#include "audio.hpp"
#include "entities/bat.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "entities/money.hpp"
#include "state.hpp"

#include <cmath>
#include <random>

namespace splonks::entities::breakaway_container {

namespace {

unsigned int RandomPercent() {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<unsigned int> distribution(0, 99);
    return distribution(generator);
}

bool EntityJustCollided(Entity& entity) {
    constexpr unsigned int kCollidedTriggerCooldown = 4;

    const bool trigger =
        !entity.collided_last_frame && entity.collided && entity.collided_trigger_cooldown == 0;

    if (entity.collided) {
        entity.collided_trigger_cooldown = kCollidedTriggerCooldown;
    } else if (entity.collided_trigger_cooldown > 0) {
        entity.collided_trigger_cooldown -= 1;
    }

    return trigger;
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
        entity.frame_data_animator.SetAnimation(frame_data_ids::Pot);
        break;
    case EntityType::Box:
        entity.frame_data_animator.SetAnimation(frame_data_ids::Box);
        break;
    default:
        entity.frame_data_animator.SetAnimation(frame_data_ids::NoSprite);
        break;
    }
}

void StepEntityLogicAsBreakawayContainer(
    std::size_t entity_idx,
    State& state,
    Audio& audio
) {
    (void)audio;
    Entity& breakaway_container = state.entity_manager.entities[entity_idx];
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

    const bool just_collided = EntityJustCollided(breakaway_container);
    const bool moving_fast =
        std::abs(breakaway_container.vel.x) > 1.0F || std::abs(breakaway_container.vel.y) > 1.0F;
    const bool was_thrown = breakaway_container.thrown_by.has_value();
    const bool fell_far = breakaway_container.fall_distance >= static_cast<float>(kTileSize);

    if (just_collided && (was_thrown || moving_fast || fell_far)) {
        breakaway_container.health = 0;
    }
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsBreakawayContainer(
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

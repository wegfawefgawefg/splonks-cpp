#include "entities/spider.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "on_damage_effects.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::entities::spider {

namespace {

constexpr int kPassiveSpiderCooldownMinFrames = 25;
constexpr int kPassiveSpiderCooldownMaxFrames = 60;
constexpr float kPassiveSpiderHopSpeedX = 1.25F;

constexpr int kRageSpiderAggroDistance = 90;
constexpr int kGiantSpiderAggroDistance = 120;
constexpr int kAggroSpiderCooldownMinFrames = 5;
constexpr int kAggroSpiderCooldownMaxFrames = 20;
constexpr float kRageSpiderHopSpeedX = 2.5F;
constexpr float kGiantSpiderHopSpeedX = 2.5F;
constexpr float kSpiderIdleSpeedThreshold = 0.1F;

std::optional<Vec2> GetNearestPlayerDelta(const Entity& entity, const State& state) {
    if (!state.player_vid.has_value()) {
        return std::nullopt;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return std::nullopt;
    }

    const Vec2 entity_center = entity.GetCenter();
    const Vec2 player_center = GetNearestWorldPoint(state.stage, entity_center, player->GetCenter());
    return player_center - entity_center;
}

std::optional<VID> SpawnEntityAtCenter(EntityType type_, const Vec2& center, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return std::nullopt;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
    return vid;
}

void SpawnGiantSpiderLoot(const Vec2& center, State& state) {
    const int gem_count = rng::RandomIntInclusive(1, 3);
    for (int i = 0; i < gem_count; ++i) {
        EntityType gem_type = EntityType::EmeraldBig;
        switch (rng::RandomIntInclusive(1, 3)) {
        case 1:
            gem_type = EntityType::EmeraldBig;
            break;
        case 2:
            gem_type = EntityType::SapphireBig;
            break;
        case 3:
            gem_type = EntityType::RubyBig;
            break;
        }

        const std::optional<VID> gem_vid = SpawnEntityAtCenter(gem_type, center, state);
        if (!gem_vid.has_value()) {
            continue;
        }

        Entity* const gem = state.entity_manager.GetEntityMut(*gem_vid);
        if (gem != nullptr) {
            gem->vel = Vec2::New(
                rng::RandomFloat(-2.0F, 2.0F),
                -2.0F
            );
        }
    }

    SpawnEntityAtCenter(EntityType::Paste, center, state);
}

void OnDeathAsGiantSpider(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& giant_spider = state.entity_manager.entities[entity_idx];
    SpawnDamageEffectAnimationBurst(frame_data_ids::BloodBall, giant_spider.GetCenter(), state);
    SpawnGiantSpiderLoot(giant_spider.GetCenter(), state);
}

void FaceTowardNearestPlayer(Entity& entity, const State& state) {
    const std::optional<Vec2> player_delta = GetNearestPlayerDelta(entity, state);
    if (!player_delta.has_value()) {
        return;
    }

    if (player_delta->x < 0.0F) {
        entity.facing = LeftOrRight::Left;
    } else if (player_delta->x > 0.0F) {
        entity.facing = LeftOrRight::Right;
    }
}

void StepPassiveSpider(Entity& entity) {
    if (entity.condition != EntityCondition::Normal) {
        return;
    }

    TrySetAnimation(entity, EntityDisplayState::Neutral);
    if (!entity.grounded) {
        return;
    }

    if (entity.counter_a > 0.0F) {
        entity.counter_a -= 1.0F;
        if (std::abs(entity.vel.x) < kSpiderIdleSpeedThreshold) {
            entity.vel.x = 0.0F;
        }
        return;
    }

    if (rng::RandomIntInclusive(0, 1) == 0) {
        entity.facing = entity.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
    }

    entity.vel.y = -static_cast<float>(rng::RandomIntInclusive(2, 4));
    entity.vel.x = entity.facing == LeftOrRight::Left ? -kPassiveSpiderHopSpeedX : kPassiveSpiderHopSpeedX;
    entity.counter_a = static_cast<float>(rng::RandomIntInclusive(
        kPassiveSpiderCooldownMinFrames,
        kPassiveSpiderCooldownMaxFrames
    ));
}

void TryHopTowardPlayer(
    Entity& entity,
    const State& state,
    int aggro_distance,
    float hop_speed_x,
    int hop_speed_y_min,
    int hop_speed_y_max
) {
    const std::optional<Vec2> player_delta = GetNearestPlayerDelta(entity, state);
    if (!player_delta.has_value() || Length(*player_delta) > static_cast<float>(aggro_distance)) {
        entity.counter_a = static_cast<float>(rng::RandomIntInclusive(
            kAggroSpiderCooldownMinFrames,
            kAggroSpiderCooldownMaxFrames
        ));
        return;
    }

    FaceTowardNearestPlayer(entity, state);
    entity.vel.y = -static_cast<float>(rng::RandomIntInclusive(hop_speed_y_min, hop_speed_y_max));
    entity.vel.x = entity.facing == LeftOrRight::Left ? -hop_speed_x : hop_speed_x;
    entity.counter_a = static_cast<float>(rng::RandomIntInclusive(
        kAggroSpiderCooldownMinFrames,
        kAggroSpiderCooldownMaxFrames
    ));
}

void StepAggroSpider(
    Entity& entity,
    const State& state,
    int aggro_distance,
    float hop_speed_x,
    int hop_speed_y_min,
    int hop_speed_y_max
) {
    if (entity.condition != EntityCondition::Normal) {
        return;
    }

    TrySetAnimation(entity, EntityDisplayState::Neutral);
    if (!entity.grounded) {
        return;
    }

    if (entity.counter_a > 0.0F) {
        entity.counter_a -= 1.0F;
        if (std::abs(entity.vel.x) < kSpiderIdleSpeedThreshold) {
            entity.vel.x = 0.0F;
        }
        return;
    }

    TryHopTowardPlayer(entity, state, aggro_distance, hop_speed_x, hop_speed_y_min, hop_speed_y_max);
}

} // namespace

void StepEntityLogicAsSpider(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& spider = state.entity_manager.entities[entity_idx];
    StepPassiveSpider(spider);
}

void StepEntityLogicAsRageSpider(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& rage_spider = state.entity_manager.entities[entity_idx];
    StepAggroSpider(rage_spider, state, kRageSpiderAggroDistance, kRageSpiderHopSpeedX, 2, 5);
}

void StepEntityLogicAsGiantSpider(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& giant_spider = state.entity_manager.entities[entity_idx];
    StepAggroSpider(giant_spider, state, kGiantSpiderAggroDistance, kGiantSpiderHopSpeedX, 3, 6);
}

extern const EntityArchetype kSpiderArchetype{
    .type_ = EntityType::Spider,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .vanish_on_death = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kPassiveSpiderCooldownMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .collide_sound = SoundEffect::Thud,
    .step_logic = StepEntityLogicAsSpider,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Spider),
};

extern const EntityArchetype kRageSpiderArchetype{
    .type_ = EntityType::RageSpider,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .vanish_on_death = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kAggroSpiderCooldownMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .collide_sound = SoundEffect::Thud,
    .step_logic = StepEntityLogicAsRageSpider,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::RageSpider),
};

extern const EntityArchetype kGiantSpiderArchetype{
    .type_ = EntityType::GiantSpider,
    .size = Vec2::New(32.0F, 32.0F),
    .health = 10,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = true,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kAggroSpiderCooldownMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .collide_sound = SoundEffect::Thud,
    .on_death = OnDeathAsGiantSpider,
    .step_logic = StepEntityLogicAsGiantSpider,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GiantSpider),
};

} // namespace splonks::entities::spider

#include "entities/spider_hang.hpp"

#include "entity/archetype.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::entities::spider_hang {

namespace {

constexpr int kSpiderDropDistance = 90;
constexpr float kSpiderDropXTolerance = 8.0F;
constexpr int kGiantSpiderDropDistance = 90;
constexpr float kGiantSpiderDropXTolerance = 8.0F;

bool HasCeilingSupport(const Entity& entity, const State& state) {
    const AABB aabb = entity.GetAABB();
    const IVec2 sample_pos = ToIVec2(Vec2::New((aabb.tl.x + aabb.br.x) * 0.5F, aabb.tl.y - 1.0F));
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, sample_pos);
    return tile_query.has_value() && tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile);
}

std::optional<Vec2> GetPlayerDeltaBelow(const Entity& entity, const State& state, int max_distance) {
    if (!state.player_vid.has_value()) {
        return std::nullopt;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return std::nullopt;
    }

    const Vec2 entity_center = entity.GetCenter();
    const Vec2 player_center = GetNearestWorldPoint(state.stage, entity_center, player->GetCenter());
    const Vec2 delta = player_center - entity_center;
    if (delta.y <= 0.0F || Length(delta) >= static_cast<float>(max_distance)) {
        return std::nullopt;
    }
    return delta;
}

void ConvertHangEntityToLiveSpider(std::size_t entity_idx, State& state, EntityType live_type) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& hang_spider = state.entity_manager.entities[entity_idx];
    const Vec2 pos = hang_spider.pos;
    const Vec2 vel = hang_spider.vel;
    const std::uint32_t health = hang_spider.health;
    const std::optional<VID> thrown_by = hang_spider.thrown_by;

    SetEntityAs(hang_spider, live_type);
    hang_spider.pos = pos;
    hang_spider.vel = vel;
    hang_spider.health = health;
    hang_spider.thrown_by = thrown_by;
    hang_spider.grounded = false;

    if (const std::optional<Vec2> player_delta = GetPlayerDeltaBelow(hang_spider, state, 9999)) {
        if (player_delta->x < 0.0F) {
            hang_spider.facing = LeftOrRight::Left;
        } else if (player_delta->x > 0.0F) {
            hang_spider.facing = LeftOrRight::Right;
        }
    }
}

EntityDamageEffectResult OnDamageAsHangSpider(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied,
    EntityType live_type
) {
    (void)audio;
    (void)damage_type;
    (void)amount;

    if (!damage_applied || entity_idx >= state.entity_manager.entities.size()) {
        return EntityDamageEffectResult::None;
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.health == 0) {
        return EntityDamageEffectResult::None;
    }

    ConvertHangEntityToLiveSpider(entity_idx, state, live_type);
    return EntityDamageEffectResult::None;
}

void StepHangSpider(
    std::size_t entity_idx,
    State& state,
    int drop_distance,
    float drop_x_tolerance,
    EntityType live_type,
    bool convert_when_hurt
) {
    Entity& hang_spider = state.entity_manager.entities[entity_idx];
    if (hang_spider.condition == EntityCondition::Dead) {
        return;
    }

    const std::optional<Vec2> player_delta = GetPlayerDeltaBelow(hang_spider, state, drop_distance);
    const bool player_below = player_delta.has_value() && std::abs(player_delta->x) < drop_x_tolerance;
    const bool hurt_but_alive = convert_when_hurt && hang_spider.health < GetEntityArchetype(hang_spider.type_).health;
    if (hurt_but_alive || !HasCeilingSupport(hang_spider, state) || player_below) {
        ConvertHangEntityToLiveSpider(entity_idx, state, live_type);
    }
}

EntityDamageEffectResult OnDamageAsSpiderHang(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    return OnDamageAsHangSpider(
        entity_idx,
        state,
        audio,
        damage_type,
        amount,
        damage_applied,
        EntityType::Spider
    );
}

EntityDamageEffectResult OnDamageAsRageSpiderHang(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    return OnDamageAsHangSpider(
        entity_idx,
        state,
        audio,
        damage_type,
        amount,
        damage_applied,
        EntityType::RageSpider
    );
}

EntityDamageEffectResult OnDamageAsGiantSpiderHang(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    return OnDamageAsHangSpider(
        entity_idx,
        state,
        audio,
        damage_type,
        amount,
        damage_applied,
        EntityType::GiantSpider
    );
}

} // namespace

void StepEntityLogicAsSpiderHang(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    StepHangSpider(entity_idx, state, kSpiderDropDistance, kSpiderDropXTolerance, EntityType::Spider, false);
}

void StepEntityLogicAsRageSpiderHang(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    StepHangSpider(
        entity_idx,
        state,
        kSpiderDropDistance,
        kSpiderDropXTolerance,
        EntityType::RageSpider,
        false
    );
}

void StepEntityLogicAsGiantSpiderHang(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    StepHangSpider(
        entity_idx,
        state,
        kGiantSpiderDropDistance,
        kGiantSpiderDropXTolerance,
        EntityType::GiantSpider,
        true
    );
}

extern const EntityArchetype kSpiderHangArchetype{
    .type_ = EntityType::SpiderHang,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .on_damage = OnDamageAsSpiderHang,
    .step_logic = StepEntityLogicAsSpiderHang,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SpiderHang),
};

extern const EntityArchetype kRageSpiderHangArchetype{
    .type_ = EntityType::RageSpiderHang,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .on_damage = OnDamageAsRageSpiderHang,
    .step_logic = StepEntityLogicAsRageSpiderHang,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::RageSpiderHang),
};

extern const EntityArchetype kGiantSpiderHangArchetype{
    .type_ = EntityType::GiantSpiderHang,
    .size = Vec2::New(32.0F, 32.0F),
    .health = 10,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .on_damage = OnDamageAsGiantSpiderHang,
    .step_logic = StepEntityLogicAsGiantSpiderHang,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GiantSpiderHang),
};

} // namespace splonks::entities::spider_hang

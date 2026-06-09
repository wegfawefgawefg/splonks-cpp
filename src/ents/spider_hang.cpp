#include "ents/spider_hang.hpp"

#include "ent/spec.hpp"
#include "ents/spider.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::ents::spider_hang {

namespace {

constexpr int kSpiderDropDistance = 90;
constexpr float kSpiderDropXTolerance = 8.0F;
constexpr int kGiantSpiderDropDistance = 90;
constexpr float kGiantSpiderDropXTolerance = 8.0F;

bool HasCeilingSupport(const Ent& ent, const State& state) {
    const sim::FxAABB aabb = ent.GetSimAABB();
    const sim::FxVec2 sample_pos = sim::FxVec2{
        aabb.center().x,
        aabb.tl.y - sim::Scalar::from_pixels(1),
    };
    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, sample_pos);
    return tile_query.has_value() && tile_query->tile != nullptr && IsTileCollidable(*tile_query->tile);
}

std::optional<sim::FxVec2> GetPlayerDeltaBelow(const Ent& ent, const State& state, int max_distance) {
    const Ent* const player = FindNearestPlayer(state, ent.GetSimCenter(), false);
    if (player == nullptr || player->condition == EntCondition::Dead) {
        return std::nullopt;
    }

    const sim::FxVec2 ent_center = ent.GetSimCenter();
    const sim::FxVec2 player_center = GetNearestWorldPoint(state.stage, ent_center, player->GetSimCenter());
    const sim::FxVec2 delta = player_center - ent_center;
    if (delta.y <= sim::Scalar::zero() ||
        gfxp::length_sq(delta) >= sim::Scalar::from_int(max_distance * max_distance)) {
        return std::nullopt;
    }
    return delta;
}

void ConvertHangEntToLiveSpider(std::size_t ent_idx, State& state, EntType live_type) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& hang_spider = state.ents.ents[ent_idx];
    const sim::FxVec2 pos = hang_spider.pos;
    const sim::FxVec2 vel = hang_spider.vel;
    const std::uint32_t health = hang_spider.health;
    const std::optional<VID> thrown_by = hang_spider.thrown_by;

    SetEntAs(hang_spider, live_type);
    hang_spider.pos = pos;
    hang_spider.vel = vel;
    hang_spider.health = health;
    hang_spider.thrown_by = thrown_by;
    hang_spider.grounded = false;

    if (const std::optional<sim::FxVec2> player_delta = GetPlayerDeltaBelow(hang_spider, state, 9999)) {
        if (player_delta->x < sim::Scalar::zero()) {
            hang_spider.facing = Side::Left;
        } else if (player_delta->x > sim::Scalar::zero()) {
            hang_spider.facing = Side::Right;
        }
    }
}

EntDamageEffectResult OnDamageAsHangSpider(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied,
    EntType live_type
) {
    (void)audio;
    (void)damage_type;
    (void)amount;

    if (!damage_applied || ent_idx >= state.ents.ents.size()) {
        return EntDamageEffectResult::None;
    }

    const Ent& ent = state.ents.ents[ent_idx];
    if (ent.health == 0) {
        return EntDamageEffectResult::None;
    }

    ConvertHangEntToLiveSpider(ent_idx, state, live_type);
    return EntDamageEffectResult::None;
}

void StepHangSpider(
    std::size_t ent_idx,
    State& state,
    int drop_distance,
    float drop_x_tolerance,
    EntType live_type,
    bool convert_when_hurt
) {
    Ent& hang_spider = state.ents.ents[ent_idx];
    if (hang_spider.condition == EntCondition::Dead) {
        return;
    }

    const std::optional<sim::FxVec2> player_delta = GetPlayerDeltaBelow(hang_spider, state, drop_distance);
    const bool player_below =
        player_delta.has_value() && player_delta->x.abs() < ToFxScalar(drop_x_tolerance);
    const bool hurt_but_alive = convert_when_hurt && hang_spider.health < GetEntSpec(hang_spider.type_).health;
    if (hurt_but_alive || !HasCeilingSupport(hang_spider, state) || player_below) {
        ConvertHangEntToLiveSpider(ent_idx, state, live_type);
    }
}

EntDamageEffectResult OnDamageAsSpiderHang(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    return OnDamageAsHangSpider(
        ent_idx,
        state,
        audio,
        damage_type,
        amount,
        damage_applied,
        EntType::Spider
    );
}

EntDamageEffectResult OnDamageAsRageSpiderHang(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    return OnDamageAsHangSpider(
        ent_idx,
        state,
        audio,
        damage_type,
        amount,
        damage_applied,
        EntType::RageSpider
    );
}

EntDamageEffectResult OnDamageAsGiantSpiderHang(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    return OnDamageAsHangSpider(
        ent_idx,
        state,
        audio,
        damage_type,
        amount,
        damage_applied,
        EntType::GiantSpider
    );
}

} // namespace

void StepEntLogicAsSpiderHang(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    StepHangSpider(ent_idx, state, kSpiderDropDistance, kSpiderDropXTolerance, EntType::Spider, false);
}

void StepEntLogicAsRageSpiderHang(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    StepHangSpider(
        ent_idx,
        state,
        kSpiderDropDistance,
        kSpiderDropXTolerance,
        EntType::RageSpider,
        false
    );
}

void StepEntLogicAsGiantSpiderHang(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    StepHangSpider(
        ent_idx,
        state,
        kGiantSpiderDropDistance,
        kGiantSpiderDropXTolerance,
        EntType::GiantSpider,
        true
    );
}

extern const EntSpec kSpiderHangSpec{
    .type_ = EntType::SpiderHang,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .on_damage = OnDamageAsSpiderHang,
    .step_logic = StepEntLogicAsSpiderHang,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SpiderHang),
};

extern const EntSpec kRageSpiderHangSpec{
    .type_ = EntType::RageSpiderHang,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .on_damage = OnDamageAsRageSpiderHang,
    .step_logic = StepEntLogicAsRageSpiderHang,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::RageSpiderHang),
};

extern const EntSpec kGiantSpiderHangSpec{
    .type_ = EntType::GiantSpiderHang,
    .size = EntSpecSize(32.0F, 32.0F),
    .health = 10,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .on_death = ents::spider::OnDeathAsGiantSpider,
    .on_damage = OnDamageAsGiantSpiderHang,
    .step_logic = StepEntLogicAsGiantSpiderHang,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GiantSpiderHang),
};

} // namespace splonks::ents::spider_hang

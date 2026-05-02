#include "entities/debug_moving_light.hpp"

#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

#include <cmath>

namespace splonks::entities::debug_moving_light {

namespace {

void StepEntityLogicAsDebugMovingLight(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& light = state.entity_manager.entities[entity_idx];
    light.counter_a += light.counter_b;

    const Vec2 home = Vec2::New(static_cast<float>(light.point_a.x), static_cast<float>(light.point_a.y));
    const float x = std::cos(light.counter_a) * light.threshold_a;
    const float y = std::sin(light.counter_a * 0.73F + light.counter_c) * light.threshold_b;
    light.SetCenter(home + Vec2::New(x, y));
    light.rotation += 0.03F + (light.counter_b * 0.35F);
}

} // namespace

extern const EntityArchetype kDebugMovingLightArchetype{
    .type_ = EntityType::DebugMovingLight,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .self_light = 0.8F,
    .light_strength = 1.2F,
    .light_color = Color3::White(),
    .light_radius = 8,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsDebugMovingLight,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Lantern),
};

} // namespace splonks::entities::debug_moving_light

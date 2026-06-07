#include "ents/debug_moving_light.hpp"

#include "ent/spec.hpp"
#include "aframe_id.hpp"
#include "state.hpp"

#include <cmath>

namespace splonks::ents::debug_moving_light {

namespace {

void StepEntLogicAsDebugMovingLight(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& light = state.ents.ents[ent_idx];
    light.counter_a += light.counter_b;

    const Vec2 home = Vec2::New(static_cast<float>(light.point_a.x), static_cast<float>(light.point_a.y));
    const float x = std::cos(light.counter_a) * light.threshold_a;
    const float y = std::sin(light.counter_a * 0.73F + light.counter_c) * light.threshold_b;
    light.SetCenter(home + Vec2::New(x, y));
    light.rotation = sim::ToSimScalar(
        sim::ToRenderScalar(light.rotation) + 0.03F + (light.counter_b * 0.35F)
    );
}

} // namespace

extern const EntSpec kDebugMovingLightSpec{
    .type_ = EntType::DebugMovingLight,
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
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .step_logic = StepEntLogicAsDebugMovingLight,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Lantern),
};

} // namespace splonks::ents::debug_moving_light

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

    const FVec2 home = FVec2::New(static_cast<float>(light.point_a.x), static_cast<float>(light.point_a.y));
    const float counter_a = ToFloat(light.counter_a);
    const float counter_b = ToFloat(light.counter_b);
    const float counter_c = ToFloat(light.counter_c);
    const float x = std::cos(counter_a) * ToFloat(light.threshold_a);
    const float y = std::sin(counter_a * 0.73F + counter_c) *
                    ToFloat(light.threshold_b);
    light.SetCenter(ToFxVec2(home + FVec2::New(x, y)));
    light.rotation = ToFxScalar(
        ToFloat(light.rotation) + 0.03F + (counter_b * 0.35F)
    );
}

} // namespace

extern const EntSpec kDebugMovingLightSpec{
    .type_ = EntType::DebugMovingLight,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .self_light = ToFxScalar(0.8F),
    .light_strength = ToFxScalar(1.2F),
    .light_color = ToFxColor3(Color3::White()),
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

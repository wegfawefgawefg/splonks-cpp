#include "ents/crystal_skull.hpp"

#include "ent/spec.hpp"
#include "ents/gold_idol.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"

namespace splonks::ents::crystal_skull {

namespace {

constexpr std::uint32_t kCrystalSkullExitValue = 15000;
constexpr std::uint32_t kCrystalSkullShopValue = 30000;

} // namespace

extern const EntSpec kCrystalSkullSpec{
    .type_ = EntType::CrystalSkull,
    .size = EntSpecSize(12.0F, 12.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_a = EntSpecCounter(static_cast<float>(kCrystalSkullExitValue)),
    .counter_b = EntSpecCounter(static_cast<float>(kCrystalSkullShopValue)),
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .step_logic = gold_idol::StepEntLogicAsGoldIdol,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::CrystalIdol),
};

} // namespace splonks::ents::crystal_skull

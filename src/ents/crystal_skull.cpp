#include "entities/crystal_skull.hpp"

#include "entity/archetype.hpp"
#include "entities/gold_idol.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::crystal_skull {

namespace {

constexpr std::uint32_t kCrystalSkullExitValue = 15000;
constexpr std::uint32_t kCrystalSkullShopValue = 30000;

} // namespace

extern const EntityArchetype kCrystalSkullArchetype{
    .type_ = EntityType::CrystalSkull,
    .size = Vec2::New(12.0F, 12.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kCrystalSkullExitValue),
    .counter_b = static_cast<float>(kCrystalSkullShopValue),
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .step_logic = gold_idol::StepEntityLogicAsGoldIdol,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::CrystalIdol),
};

} // namespace splonks::entities::crystal_skull

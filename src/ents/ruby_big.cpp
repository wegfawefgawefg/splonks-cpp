#include "ents/ruby_big.hpp"

#include "audio_emitters.hpp"
#include "effects/treasure_pickup.hpp"
#include "ents/common/common.hpp"

#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "world_ops.hpp"

namespace splonks::ents::ruby_big {

namespace {

common::ContactResult OnEntContactAsRubyBig(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr ||
        !common::CanCollectPickupFromContact(ent_idx, other_ent_idx, state)) {
        return common::ContactResult{};
    }
    Ent& collector = state.ents.ents[other_ent_idx];
    const Ent& gem = state.ents.ents[ent_idx];
    collector.money += 1600;
    (void)PlayEntCenterSoundEmitter(state, gem, audio_asset_ids::GoldStack);
    effects::SpawnTreasurePickupSparkles(gem, state, Color3::New(1.0F, 0.16F, 0.26F), 8);
    common::DeactivateCollectedPickup(ent_idx, state, *graphics);
    return common::ContactResult{};
}

} // namespace

extern const EntSpec kRubyBigSpec{
    .type_ = EntType::RubyBig,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .self_light = 0.32F,
    .light_strength = 0.55F,
    .light_color = Color3::New(1.0F, 0.16F, 0.26F),
    .light_radius = 6,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsRubyBig,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::RubyBig),
};

} // namespace splonks::ents::ruby_big

#include "ents/sapphire_big.hpp"

#include "audio_emitters.hpp"
#include "effects/treasure_pickup.hpp"
#include "ents/common/common.hpp"

#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "world_ops.hpp"

namespace splonks::ents::sapphire_big {

namespace {

common::ContactResult OnEntContactAsSapphireBig(
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
    collector.money += 1200;
    (void)PlayEntCenterSoundEmitter(state, gem, audio_asset_ids::GoldStack);
    effects::SpawnTreasurePickupSparkles(gem, state, Color3::New(0.24F, 0.46F, 1.0F), 7);
    common::DeactivateCollectedPickup(ent_idx, state, *graphics);
    return common::ContactResult{};
}

} // namespace

extern const EntSpec kSapphireBigSpec{
    .type_ = EntType::SapphireBig,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .self_light = 0.28F,
    .light_strength = 0.45F,
    .light_color = Color3::New(0.24F, 0.46F, 1.0F),
    .light_radius = 5,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsSapphireBig,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SapphireBig),
};

} // namespace splonks::ents::sapphire_big

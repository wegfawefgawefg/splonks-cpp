#include "ents/gear_items.hpp"

#include "audio_emitters.hpp"
#include "ent/spec.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdint>

namespace splonks::ents::gear_items {

namespace {

constexpr float kParachuteMaxFallSpeed = 1.35F;
constexpr float kParachuteVisualOffsetY = -12.0F;
constexpr float kCapeMaxFallSpeed = 1.35F;

common::ContactResult OnEntContactAsInventoryPickup(
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
    const Ent& pickup = state.ents.ents[ent_idx];
    if (!TryCollectInventoryPickup(state, collector, pickup)) {
        return common::ContactResult{};
    }

    (void)PlayEntCenterSoundEmitter(state, pickup, audio_asset_ids::Equip);
    if (pickup.type_ == EntType::SpringShoes) {
        (void)PlayEntCenterSoundEmitter(state, pickup, audio_asset_ids::SpringShoe);
    }
    common::DeactivateCollectedPickup(ent_idx, state, *graphics);
    return common::ContactResult{};
}

Ent* GetOpenParachuteVisual(Ent& owner, State& state) {
    if (!owner.ent_b.has_value()) {
        return nullptr;
    }
    Ent* const parachute = state.ents.GetEntMut(*owner.ent_b);
    if (parachute == nullptr || !parachute->active || parachute->type_ != EntType::Parachute ||
        parachute->aframe_animator.anim_id != aframe_ids::OpenParachute) {
        owner.ent_b.reset();
        return nullptr;
    }
    return parachute;
}

std::uint32_t GetParachuteDeployFallFrames(const State& state) {
    return static_cast<std::uint32_t>(std::max(0, state.player_tuning.fall_damage_light_frames));
}

void ClearOpenParachuteVisual(Ent& owner, State& state, const Graphics& graphics) {
    Ent* const parachute = GetOpenParachuteVisual(owner, state);
    if (parachute == nullptr) {
        return;
    }
    if (!world_ops::DeactivateEnt(state, parachute->vid)) {
        return;
    }
    owner.ent_b.reset();
    state.UpdateSidForEnt(parachute->vid.id, graphics);
}

void UpdateOpenParachuteVisual(Ent& owner, State& state, const Graphics& graphics) {
    Ent* parachute = GetOpenParachuteVisual(owner, state);
    bool spawned_visual = false;
    if (parachute == nullptr) {
        parachute = world_ops::SpawnEnt(
            state,
            EntType::Parachute,
            [](Ent& spawned) {
                SetAnim(spawned, aframe_ids::OpenParachute);
                spawned.has_physics = false;
                spawned.can_collide = false;
                spawned.can_be_hit = false;
                spawned.can_be_picked_up = false;
                spawned.draw_layer = DrawLayer::Background;
            }
        );
        if (parachute == nullptr) {
            return;
        }
        owner.ent_b = parachute->vid;
        spawned_visual = true;
    }

    const Vec2 owner_visual_center =
        common::GetVisualCenterForEnt(owner, graphics, owner.GetCenter());
    parachute->SetCenter(owner_visual_center + Vec2::New(0.0F, kParachuteVisualOffsetY));
    parachute->vel = Vec2::New(0.0F, 0.0F);
    parachute->acc = Vec2::New(0.0F, 0.0F);
    state.UpdateSidForEnt(parachute->vid.id, graphics);
    (void)spawned_visual;
}

void StepEquippedParachute(Ent& owner, State& state, const Graphics& graphics) {
    if (owner.grounded || owner.condition != EntCondition::Normal) {
        ClearOpenParachuteVisual(owner, state, graphics);
        return;
    }

    const bool already_open = GetOpenParachuteVisual(owner, state) != nullptr;
    if (!already_open) {
        if (!HasEffect(owner, EffectId::Parachute) ||
            owner.fall_timer < GetParachuteDeployFallFrames(state)) {
            return;
        }
        RemoveEffect(owner, EffectId::Parachute);
    }

    owner.vel.y = std::min(owner.vel.y, kParachuteMaxFallSpeed);
    owner.fall_timer = 0;
    UpdateOpenParachuteVisual(owner, state, graphics);
}

AFrameId GetCapeAnim(const Ent& cape, const State& state) {
    const bool open = cape.counter_a > 0.0F;
    if (cape.attach_mode == AttachMode::Back && cape.held_by_vid.has_value()) {
        const Ent* const holder = state.ents.GetEnt(*cape.held_by_vid);
        if (holder != nullptr) {
            if (holder->IsHanging()) {
                return open ? aframe_ids::CapeSideOpen : aframe_ids::CapeSide;
            }
            if (holder->IsClimbing()) {
                return open ? aframe_ids::CapeBackOpen : aframe_ids::CapeBack;
            }
        }
        return open ? aframe_ids::CapeOpen : aframe_ids::Cape;
    }

    if (cape.attach_mode == AttachMode::Held || cape.held_by_vid.has_value()) {
        return open ? aframe_ids::CapeSideOpen : aframe_ids::CapeSide;
    }
    return aframe_ids::CapeClosed;
}

void OnUseAsCape(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& cape = state.ents.ents[ent_idx];
    cape.counter_a = 0.0F;
    if (!cape.use_state.down ||
        !cape.use_state.user_vid.has_value()) {
        return;
    }
    if (cape.use_state.source != AttachMode::Back &&
        cape.use_state.source != AttachMode::Held) {
        return;
    }

    Ent* const holder = state.ents.GetEntMut(*cape.use_state.user_vid);
    if (holder == nullptr || holder->condition != EntCondition::Normal) {
        return;
    }

    cape.counter_a = 1.0F;
    if (!holder->grounded && holder->vel.y > 0.0F) {
        holder->vel.y = std::min(holder->vel.y, kCapeMaxFallSpeed);
        holder->fall_timer = 0;
    }
}

void StepEntLogicAsCape(
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

    Ent& cape = state.ents.ents[ent_idx];
    if (!cape.use_state.down) {
        cape.counter_a = 0.0F;
    }
    SetAnim(cape, GetCapeAnim(cape, state));
}

} // namespace

void StepEquippedPassiveItems(std::size_t ent_idx, State& state, Graphics& graphics) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& ent = state.ents.ents[ent_idx];
    StepEquippedParachute(ent, state, graphics);
}

void ClearEquippedPassiveItemVisuals(Ent& ent, State& state, const Graphics& graphics) {
    ClearOpenParachuteVisual(ent, state, graphics);
}

extern const EntSpec kCapeSpec{
    .type_ = EntType::Cape,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_go_on_back = true,
    .can_be_stunned = false,
    .predict_local_attach_use = true,
    .predict_attach_use_pres = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .on_use = OnUseAsCape,
    .step_logic = StepEntLogicAsCape,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::CapePickup),
};
extern const EntSpec kGlovesSpec{
    .type_ = EntType::Gloves,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .pickup_effect = EffectId::Gloves,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Gloves),
};
extern const EntSpec kSpectaclesSpec{
    .type_ = EntType::Spectacles,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .pickup_effect = EffectId::Spectacles,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Spectacles),
};
extern const EntSpec kMittSpec{
    .type_ = EntType::Mitt,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .pickup_effect = EffectId::Mitt,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Mitt),
};
extern const EntSpec kPasteSpec{
    .type_ = EntType::Paste,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SpiderMilk),
};
extern const EntSpec kSpringShoesSpec{
    .type_ = EntType::SpringShoes,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .pickup_effect = EffectId::SpringShoes,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SpringShoes),
};
extern const EntSpec kSpikeShoesSpec{
    .type_ = EntType::SpikeShoes,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .pickup_effect = EffectId::SpikeShoes,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SpikeShoes),
};
extern const EntSpec kBombBoxSpec{
    .type_ = EntType::BombBox,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::BombBox),
};
extern const EntSpec kBombBagSpec{
    .type_ = EntType::BombBag,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::BombBag),
};
extern const EntSpec kCompassSpec{
    .type_ = EntType::Compass,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .pickup_effect = EffectId::Compass,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Compass),
};
extern const EntSpec kParachuteSpec{
    .type_ = EntType::Parachute,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .damage_vuln = DamageVuln::Vulnerable,
    .pickup_effect = EffectId::Parachute,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::PackedParachute),
};
extern const EntSpec kRopePileSpec{
    .type_ = EntType::RopePile,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .on_ent_contact = OnEntContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::RopePile),
};

} // namespace splonks::ents::gear_items

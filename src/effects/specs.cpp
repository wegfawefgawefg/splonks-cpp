#include "effects.hpp"

#include "audio_emitters.hpp"
#include "effects/render.hpp"
#include "ent.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/meathead.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "tile.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace splonks {

namespace {

const EffectModifier kNoGravityUntilContactModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::GravityScale,
        .op = EffectModifierOp::Override,
        .value = sim::Scalar::zero(),
    },
};

const EffectModifier kSpectaclesModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::HiddenTreasureVisibility,
        .op = EffectModifierOp::Max,
        .value = sim::Scalar::from_int(1),
    },
};

const EffectModifier kUdjatEyeModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::HiddenTreasureVisibility,
        .op = EffectModifierOp::Max,
        .value = sim::Scalar::from_int(1),
    },
};

const EffectModifier kSpikeShoesModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::SpikeDamageTaken,
        .op = EffectModifierOp::Override,
        .value = sim::Scalar::zero(),
    },
    EffectModifier{
        .target = EffectModifierTarget::StompDamage,
        .op = EffectModifierOp::Max,
        .value = sim::Scalar::from_int(2),
    },
};

const EffectModifier kSpringShoesModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::JumpImpulse,
        .op = EffectModifierOp::Add,
        .value = sim::Scalar::from_int(1),
    },
    EffectModifier{
        .target = EffectModifierTarget::StompBounceImpulse,
        .op = EffectModifierOp::Add,
        .value = sim::Scalar::from_int(1),
    },
};

const EffectModifier kMittModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::ThrowHorizontalBoost,
        .op = EffectModifierOp::Add,
        .value = sim::Scalar::from_int(6),
    },
};

const EffectModifier kInWaterModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::GravityScale,
        .op = EffectModifierOp::Multiply,
        .value = sim::ToSimScalar(0.35F),
    },
    EffectModifier{
        .target = EffectModifierTarget::VelocityDampingX,
        .op = EffectModifierOp::Multiply,
        .value = sim::ToSimScalar(0.82F),
    },
    EffectModifier{
        .target = EffectModifierTarget::VelocityDampingY,
        .op = EffectModifierOp::Multiply,
        .value = sim::ToSimScalar(0.55F),
    },
    EffectModifier{
        .target = EffectModifierTarget::MoveSpeedScale,
        .op = EffectModifierOp::Multiply,
        .value = sim::ToSimScalar(0.5F),
    },
    EffectModifier{
        .target = EffectModifierTarget::MaxFallSpeed,
        .op = EffectModifierOp::Min,
        .value = sim::ToSimScalar(1.35F),
    },
    EffectModifier{
        .target = EffectModifierTarget::BuoyancyStrength,
        .op = EffectModifierOp::Max,
        .value = sim::ToSimScalar(0.55F),
    },
    EffectModifier{
        .target = EffectModifierTarget::FallTimerRate,
        .op = EffectModifierOp::Override,
        .value = sim::Scalar::zero(),
    },
    EffectModifier{
        .target = EffectModifierTarget::StompDamageScale,
        .op = EffectModifierOp::Override,
        .value = sim::Scalar::zero(),
    },
    EffectModifier{
        .target = EffectModifierTarget::SwimImpulse,
        .op = EffectModifierOp::Max,
        .value = sim::ToSimScalar(8.70F),
    },
};

template <std::size_t N>
std::vector<EffectModifier> MakeModifierVector(const EffectModifier (&modifiers)[N]) {
    return std::vector<EffectModifier>(std::begin(modifiers), std::end(modifiers));
}

std::string FormatEffectInt(int value) {
    char text[16];
    std::snprintf(text, sizeof(text), "%d", value);
    return std::string(text);
}

std::optional<std::string> FormatEffectCountText(const EffectInstance& effect) {
    return FormatEffectInt(effect.count);
}

std::optional<std::string> FormatMeatheadCountdownText(const EffectInstance& effect) {
    constexpr std::int32_t kMeatheadPointsPerHeal = 10;
    return FormatEffectInt(std::max(1, kMeatheadPointsPerHeal - effect.count));
}

bool IsNoGravityUntilContactExpired(
    const Ent&,
    const EffectInstance&,
    const State&,
    const EffectHookContext& hook
) {
    return hook.type == EffectHookType::Grounded || hook.type == EffectHookType::BlockingContact;
}

void OnMittEffectHook(
    Ent&,
    EffectInstance&,
    State& state,
    Audio*,
    const EffectHookContext& hook
) {
    if (hook.type != EffectHookType::Throw || !hook.target_vid.has_value()) {
        return;
    }

    Ent* const thrown = state.ents.GetEntMut(*hook.target_vid);
    if (thrown == nullptr) {
        return;
    }
    thrown->grounded = false;
    thrown->collided = false;
    thrown->collided_last_frame = false;
    (void)AddEffect(*thrown, EffectId::NoGravityUntilContact);
}

std::optional<Vec2> FindEntranceRevivePos(const State& state) {
    for (unsigned int y = 0; y < state.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < state.stage.GetTileWidth(); ++x) {
            if (state.stage.GetTile(x, y) == Tile::Entrance) {
                return Vec2::New(static_cast<float>(x), static_cast<float>(y)) *
                       static_cast<float>(kTileSize);
            }
        }
    }
    return std::nullopt;
}

void SnapBackItemToOwner(Ent& owner, State& state) {
    if (!owner.back_vid.has_value()) {
        return;
    }

    Ent* const back_item = state.ents.GetEntMut(*owner.back_vid);
    if (back_item == nullptr || !back_item->active) {
        owner.back_vid.reset();
        return;
    }

    back_item->held_by_vid = owner.vid;
    back_item->attach_mode = AttachMode::Back;
    back_item->has_physics = false;
    back_item->can_collide = false;
    back_item->facing = owner.facing;
    back_item->SetCenter(owner.GetCenter());
}

void OnAnkhEffectHook(
    Ent& owner,
    EffectInstance&,
    State& state,
    Audio* audio,
    const EffectHookContext& hook
) {
    if (hook.type != EffectHookType::Death || !hook.target_vid.has_value() ||
        *hook.target_vid != owner.vid) {
        return;
    }

    ents::common::DropHeldItemFromEnt(owner, state);

    const EntSpec& spec = GetEntSpec(owner.type_);
    owner.health = spec.health;
    owner.condition = EntCondition::Normal;
    owner.last_condition = EntCondition::Normal;
    owner.stun_timer = 0;
    owner.fall_timer = 0;
    owner.vel = sim::Vec2::zero();
    owner.acc = sim::Vec2::zero();
    owner.grounded = false;
    owner.marked_for_destruction = false;
    owner.has_physics = spec.has_physics;
    owner.can_collide = spec.can_collide;
    owner.render_enabled = spec.render_enabled;
    owner.proj_contact_timer = 0;
    owner.thrown_by.reset();
    owner.thrown_immunity_timer = 0;
    owner.collided = false;
    owner.collided_last_frame = false;
    owner.coyote_time = 0;
    owner.jumped_this_frame = false;
    (void)TrySetAnim(owner, EntDisplayState::Neutral);

    if (const std::optional<Vec2> entrance_pos = FindEntranceRevivePos(state)) {
        owner.SetRenderPos(*entrance_pos);
    }
    SnapBackItemToOwner(owner, state);
    RemoveEffect(owner, EffectId::Ankh);

    if (audio != nullptr) {
        (void)PlayWorldSoundEmitter(state, owner.GetCenter(), audio_asset_ids::Present);
    }
}

const std::array<EffectSpec, static_cast<std::size_t>(EffectId::Count)> kEffectSpecs{{
    EffectSpec{
        .id = EffectId::None,
        .debug_name = "None",
        .modifiers = {},
    },
    EffectSpec{
        .id = EffectId::Gloves,
        .debug_name = "Gloves",
        .icon_anim_id = aframe_ids::Gloves,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = {},
    },
    EffectSpec{
        .id = EffectId::Spectacles,
        .debug_name = "Spectacles",
        .icon_anim_id = aframe_ids::Spectacles,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierVector(kSpectaclesModifiers),
    },
    EffectSpec{
        .id = EffectId::Compass,
        .debug_name = "Compass",
        .icon_anim_id = aframe_ids::Compass,
        .ui_kind = EffectUiKind::Passive,
        .render_world_overlay = RenderCompassWorldOverlay,
        .modifiers = {},
    },
    EffectSpec{
        .id = EffectId::Mitt,
        .debug_name = "Mitt",
        .icon_anim_id = aframe_ids::Mitt,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierVector(kMittModifiers),
        .on_hook = OnMittEffectHook,
    },
    EffectSpec{
        .id = EffectId::SpringShoes,
        .debug_name = "SpringShoes",
        .icon_anim_id = aframe_ids::SpringShoes,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierVector(kSpringShoesModifiers),
    },
    EffectSpec{
        .id = EffectId::SpikeShoes,
        .debug_name = "SpikeShoes",
        .icon_anim_id = aframe_ids::SpikeShoes,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierVector(kSpikeShoesModifiers),
    },
    EffectSpec{
        .id = EffectId::UdjatEye,
        .debug_name = "UdjatEye",
        .icon_anim_id = aframe_ids::UdjatEye,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierVector(kUdjatEyeModifiers),
    },
    EffectSpec{
        .id = EffectId::Ankh,
        .debug_name = "Ankh",
        .icon_anim_id = aframe_ids::Ankh,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = {},
        .on_hook = OnAnkhEffectHook,
    },
    EffectSpec{
        .id = EffectId::Meathead,
        .debug_name = "Meathead",
        .icon_anim_id = aframe_ids::Meathead,
        .ui_kind = EffectUiKind::Passive,
        .hud_count_text = FormatMeatheadCountdownText,
        .hud_count_anchor = HudAnchor::BottomRight,
        .modifiers = {},
        .on_hook = ents::meathead::OnMeatheadEffectHook,
    },
    EffectSpec{
        .id = EffectId::Parachute,
        .debug_name = "Parachute",
        .icon_anim_id = aframe_ids::PackedParachute,
        .ui_kind = EffectUiKind::Passive,
        .default_count = 1,
        .hud_count_text = FormatEffectCountText,
        .hud_count_anchor = HudAnchor::BottomRight,
        .modifiers = {},
    },
    EffectSpec{
        .id = EffectId::NoGravityUntilContact,
        .debug_name = "NoGravityUntilContact",
        .icon_anim_id = aframe_ids::MittNoGrav,
        .ui_kind = EffectUiKind::Temporary,
        .modifiers = MakeModifierVector(kNoGravityUntilContactModifiers),
        .should_expire = IsNoGravityUntilContactExpired,
    },
    EffectSpec{
        .id = EffectId::InWater,
        .debug_name = "InWater",
        .icon_anim_id = aframe_ids::Water,
        .ui_kind = EffectUiKind::Temporary,
        .modifiers = MakeModifierVector(kInWaterModifiers),
    },
}};

} // namespace

const EffectSpec& GetEffectSpec(EffectId id) {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= kEffectSpecs.size()) {
        return kEffectSpecs[0];
    }
    return kEffectSpecs[index];
}

const char* EffectIdToString(EffectId id) {
    return GetEffectSpec(id).debug_name;
}

} // namespace splonks

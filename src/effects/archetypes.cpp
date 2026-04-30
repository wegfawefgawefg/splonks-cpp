#include "effects.hpp"

#include "audio_emitters.hpp"
#include "effects/render.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "entities/meathead.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "tile.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

namespace splonks {

namespace {

constexpr EffectModifier kNoGravityUntilContactModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::GravityScale,
        .op = EffectModifierOp::Override,
        .value = 0.0F,
    },
};

constexpr EffectModifier kSpectaclesModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::HiddenTreasureVisibility,
        .op = EffectModifierOp::Max,
        .value = 1.0F,
    },
};

constexpr EffectModifier kUdjatEyeModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::HiddenTreasureVisibility,
        .op = EffectModifierOp::Max,
        .value = 1.0F,
    },
};

constexpr EffectModifier kSpikeShoesModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::SpikeDamageTaken,
        .op = EffectModifierOp::Override,
        .value = 0.0F,
    },
    EffectModifier{
        .target = EffectModifierTarget::StompDamage,
        .op = EffectModifierOp::Max,
        .value = 2.0F,
    },
};

constexpr EffectModifier kSpringShoesModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::JumpImpulse,
        .op = EffectModifierOp::Add,
        .value = 1.0F,
    },
    EffectModifier{
        .target = EffectModifierTarget::StompBounceImpulse,
        .op = EffectModifierOp::Add,
        .value = 1.0F,
    },
};

constexpr EffectModifier kMittModifiers[]{
    EffectModifier{
        .target = EffectModifierTarget::ThrowHorizontalBoost,
        .op = EffectModifierOp::Add,
        .value = 6.0F,
    },
};

template <std::size_t N>
std::array<EffectModifier, 4> MakeModifierArray(const EffectModifier (&modifiers)[N]) {
    static_assert(N <= 4);
    std::array<EffectModifier, 4> out{};
    for (std::size_t i = 0; i < N; ++i) {
        out[i] = modifiers[i];
    }
    return out;
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
    const Entity&,
    const EffectInstance&,
    const State&,
    const EffectEvent& event
) {
    return event.type == EffectEventType::Grounded || event.type == EffectEventType::BlockingContact;
}

void OnMittEffectEvent(
    Entity&,
    EffectInstance&,
    State& state,
    Audio*,
    const EffectEvent& event
) {
    if (event.type != EffectEventType::Throw || !event.target_vid.has_value()) {
        return;
    }

    Entity* const thrown = state.entity_manager.GetEntityMut(*event.target_vid);
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

void SnapBackItemToOwner(Entity& owner, State& state) {
    if (!owner.back_vid.has_value()) {
        return;
    }

    Entity* const back_item = state.entity_manager.GetEntityMut(*owner.back_vid);
    if (back_item == nullptr || !back_item->active) {
        owner.back_vid.reset();
        return;
    }

    back_item->held_by_vid = owner.vid;
    back_item->attachment_mode = AttachmentMode::Back;
    back_item->has_physics = false;
    back_item->can_collide = false;
    back_item->facing = owner.facing;
    back_item->SetCenter(owner.GetCenter());
}

void OnAnkhEffectEvent(
    Entity& owner,
    EffectInstance&,
    State& state,
    Audio* audio,
    const EffectEvent& event
) {
    if (event.type != EffectEventType::Death || !event.target_vid.has_value() ||
        *event.target_vid != owner.vid) {
        return;
    }

    entities::common::DropHeldItemFromEntity(owner, state);

    const EntityArchetype& archetype = GetEntityArchetype(owner.type_);
    owner.health = archetype.health;
    owner.condition = EntityCondition::Normal;
    owner.last_condition = EntityCondition::Normal;
    owner.stun_timer = 0;
    owner.fall_timer = 0;
    owner.vel = Vec2::New(0.0F, 0.0F);
    owner.acc = Vec2::New(0.0F, 0.0F);
    owner.grounded = false;
    owner.marked_for_destruction = false;
    owner.has_physics = archetype.has_physics;
    owner.can_collide = archetype.can_collide;
    owner.render_enabled = archetype.render_enabled;
    owner.projectile_contact_timer = 0;
    owner.thrown_by.reset();
    owner.thrown_immunity_timer = 0;
    owner.collided = false;
    owner.collided_last_frame = false;
    owner.coyote_time = 0;
    owner.jumped_this_frame = false;
    (void)TrySetAnimation(owner, EntityDisplayState::Neutral);

    if (const std::optional<Vec2> entrance_pos = FindEntranceRevivePos(state)) {
        owner.pos = *entrance_pos;
    }
    SnapBackItemToOwner(owner, state);
    RemoveEffect(owner, EffectId::Ankh);

    if (audio != nullptr) {
        (void)PlayWorldSoundEmitter(state, owner.GetCenter(), audio_asset_ids::Present);
    }
}

const std::array<EffectArchetype, static_cast<std::size_t>(EffectId::Count)> kEffectArchetypes{{
    EffectArchetype{
        .id = EffectId::None,
        .debug_name = "None",
    },
    EffectArchetype{
        .id = EffectId::Gloves,
        .debug_name = "Gloves",
        .icon_animation_id = frame_data_ids::Gloves,
        .ui_kind = EffectUiKind::Passive,
    },
    EffectArchetype{
        .id = EffectId::Spectacles,
        .debug_name = "Spectacles",
        .icon_animation_id = frame_data_ids::Spectacles,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierArray(kSpectaclesModifiers),
        .modifier_count = 1,
    },
    EffectArchetype{
        .id = EffectId::Compass,
        .debug_name = "Compass",
        .icon_animation_id = frame_data_ids::Compass,
        .ui_kind = EffectUiKind::Passive,
        .render_world_overlay = RenderCompassWorldOverlay,
    },
    EffectArchetype{
        .id = EffectId::Mitt,
        .debug_name = "Mitt",
        .icon_animation_id = frame_data_ids::Mitt,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierArray(kMittModifiers),
        .modifier_count = 1,
        .on_event = OnMittEffectEvent,
    },
    EffectArchetype{
        .id = EffectId::SpringShoes,
        .debug_name = "SpringShoes",
        .icon_animation_id = frame_data_ids::SpringShoes,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierArray(kSpringShoesModifiers),
        .modifier_count = 2,
    },
    EffectArchetype{
        .id = EffectId::SpikeShoes,
        .debug_name = "SpikeShoes",
        .icon_animation_id = frame_data_ids::SpikeShoes,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierArray(kSpikeShoesModifiers),
        .modifier_count = 2,
    },
    EffectArchetype{
        .id = EffectId::UdjatEye,
        .debug_name = "UdjatEye",
        .icon_animation_id = frame_data_ids::UdjatEye,
        .ui_kind = EffectUiKind::Passive,
        .modifiers = MakeModifierArray(kUdjatEyeModifiers),
        .modifier_count = 1,
    },
    EffectArchetype{
        .id = EffectId::Ankh,
        .debug_name = "Ankh",
        .icon_animation_id = frame_data_ids::Ankh,
        .ui_kind = EffectUiKind::Passive,
        .on_event = OnAnkhEffectEvent,
    },
    EffectArchetype{
        .id = EffectId::Meathead,
        .debug_name = "Meathead",
        .icon_animation_id = frame_data_ids::Meathead,
        .ui_kind = EffectUiKind::Passive,
        .hud_count_text = FormatMeatheadCountdownText,
        .hud_count_anchor = HudAnchor::BottomRight,
        .on_event = entities::meathead::OnMeatheadEffectEvent,
    },
    EffectArchetype{
        .id = EffectId::Parachute,
        .debug_name = "Parachute",
        .icon_animation_id = frame_data_ids::PackedParachute,
        .ui_kind = EffectUiKind::Passive,
        .default_count = 1,
        .hud_count_text = FormatEffectCountText,
        .hud_count_anchor = HudAnchor::BottomRight,
    },
    EffectArchetype{
        .id = EffectId::NoGravityUntilContact,
        .debug_name = "NoGravityUntilContact",
        .icon_animation_id = frame_data_ids::MittNoGrav,
        .ui_kind = EffectUiKind::Temporary,
        .modifiers = MakeModifierArray(kNoGravityUntilContactModifiers),
        .modifier_count = 1,
        .should_expire = IsNoGravityUntilContactExpired,
    },
}};

} // namespace

const EffectArchetype& GetEffectArchetype(EffectId id) {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= kEffectArchetypes.size()) {
        return kEffectArchetypes[0];
    }
    return kEffectArchetypes[index];
}

const char* EffectIdToString(EffectId id) {
    return GetEffectArchetype(id).debug_name;
}

} // namespace splonks

#pragma once

#include <array>
#include "ent.hpp"
#include "ent/callbacks.hpp"
#include "aframe_id.hpp"
#include "hud/types.hpp"
#include "fxp.hpp"

namespace splonks {

struct Graphics;
struct State;

using EntBuildHudEntry = void (*)(const Ent& ent, const State& state, HudEntrySource source, HudEntry& entry);

struct EntSpec {
    EntType type_ = EntType::None;
    FxVec2 size = FxVec2::from_pixels(8, 8);
    std::uint32_t health = 0;
    bool has_physics = true;
    bool can_collide = true;
    bool can_be_hit = true;
    bool can_receive_proj_contact = true;
    bool can_be_picked_up = true;
    bool affected_by_cobweb = true;
    bool can_collect_pickups = false;
    bool can_only_be_picked_up_if_dead_or_stunned = false;
    bool impassable = false;
    bool can_be_hung_on = true;
    bool hurt_on_contact = false;
    bool crusher_pusher = false;
    bool pushable = false;
    bool can_stomp = false;
    bool can_be_stomped = true;
    bool vanish_on_death = false;
    bool can_go_on_back = false;
    bool can_hang_ledge = false;
    bool can_be_stunned = false;
    bool stun_recovers_on_ground = true;
    bool stun_recovers_while_held = true;
    bool affected_by_ground_friction = true;
    FxScalar support_ground_friction = ToFxScalar(0.85F);
    FxScalar push_acc = FxScalar::zero();
    FxScalar throw_velocity_scale = FxScalar::from_int(1);
    FxScalar buoyancy = FxScalar::zero();
    FxScalar alpha = FxScalar::from_int(1);
    FxScalar self_light = FxScalar::zero();
    FxScalar light_strength = FxScalar::zero();
    FxColor3 light_color = ToFxColor3(Color3::White());
    int light_radius = 0;
    bool preserve_held_aim = false;
    bool predict_local_attach_use = false;
    bool predict_attach_use_pres = false;
    DrawLayer draw_layer = DrawLayer::Middle;
    bool render_enabled = true;
    Side facing = Side::Left;
    EntCondition condition = EntCondition::Normal;
    EntAiState ai_state = EntAiState::Idle;
    EntDisplayState display_state = EntDisplayState::Neutral;
    FxScalar counter_a = FxScalar::zero();
    FxScalar counter_b = FxScalar::zero();
    FxScalar counter_d = FxScalar::zero();
    DamageVuln damage_vuln = DamageVuln::Vulnerable;
    DamageType proj_contact_damage_type = DamageType::Attack;
    std::uint32_t proj_contact_damage_amount = 1;
    bool can_apply_proj_contact = true;
    std::optional<EffectId> pickup_effect = std::nullopt;
    Buyable buyable{};
    std::optional<AFrameId> damage_anim = std::nullopt;
    std::optional<AudioAssetId> damage_sound = std::nullopt;
    std::optional<AudioAssetId> collide_sound = std::nullopt;
    std::optional<AudioAssetId> death_sound = std::nullopt;
    EntOnDeath on_death = nullptr;
    EntOnDamage on_damage = nullptr;
    EntOnUse on_use = nullptr;
    EntOnInteract on_interact = nullptr;
    EntOnAreaEnter on_area_enter = nullptr;
    EntOnAreaExit on_area_exit = nullptr;
    EntOnAreaTileChanged on_area_tile_changed = nullptr;
    EntControlLogic control_logic = nullptr;
    EntStepLogic step_logic = nullptr;
    EntStepPhysics step_physics = nullptr;
    EntOnEntContact on_ent_contact = nullptr;
    EntOnTileContact on_tile_contact = nullptr;
    EntBuildHudEntry build_hud_entry = nullptr;
    std::uint32_t ent_contact_cooldown_duration = 0;
    EntLabel ent_label_a = EntLabel::None;
    Alignment alignment = Alignment::Neutral;
    const char* debug_name = "Unknown";
    AFrameAnimator aframe_animator{};
};

inline FxVec2 EntSpecSize(float width, float height) {
    return ToFxVec2(FVec2::New(width, height));
}

inline FxVec2 EntSpecSize(const FVec2& size) {
    return ToFxVec2(size);
}

const EntSpec& GetEntSpec(EntType type_);
const char* GetEntTypeName(EntType type_);
void PopulateEntSpecsTable();
void SyncEntSpecSizesFromAFrame(const Graphics& graphics);
void SetEntAs(Ent& ent, EntType type_);
AFrameId GetDefaultAnimIdForSpec(EntType type_);

} // namespace splonks

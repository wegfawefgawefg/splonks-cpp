#pragma once

#include "audio.hpp"

#include <array>
#include "ent.hpp"

namespace splonks {

struct Graphics;
struct State;

using EntOnDeath = void (*)(std::size_t ent_idx, State& state, Audio& audio);
using EntOnUse =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio);
using EntControlLogic =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio, float dt);
using EntStepLogic =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio, float dt);
using EntStepPhysics =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio, float dt);

struct EntSpec {
    EntType type_ = EntType::None;
    Vec2 size = Vec2::New(8.0F, 8.0F);
    std::uint32_t health = 0;
    bool has_physics = true;
    bool can_collide = true;
    bool can_be_picked_up = true;
    bool impassable = false;
    bool hurt_on_contact = false;
    bool crusher_pusher = false;
    bool vanish_on_death = false;
    bool can_go_on_back = false;
    bool can_hang_ledge = false;
    bool can_be_stunned = false;
    bool has_ground_friction = true;
    float throw_velocity_scale = 1.0F;
    DrawLayer draw_layer = DrawLayer::Middle;
    Side facing = Side::Left;
    EntCondition condition = EntCondition::Normal;
    EntAiState ai_state = EntAiState::Idle;
    EntDisplayState display_state = EntDisplayState::Neutral;
    float counter_a = 0.0F;
    DamageVuln damage_vuln = DamageVuln::Vulnerable;
    std::optional<EffectId> pickup_effect = std::nullopt;
    std::optional<SoundEffect> death_sound = std::nullopt;
    EntOnDeath on_death = nullptr;
    EntOnUse on_use = nullptr;
    EntControlLogic control_logic = nullptr;
    EntStepLogic step_logic = nullptr;
    EntStepPhysics step_physics = nullptr;
    EntLabel ent_label_a = EntLabel::None;
    Alignment alignment = Alignment::Neutral;
    const char* debug_name = "Unknown";
    AFrameAnimator aframe_animator{};
};

const EntSpec& GetEntSpec(EntType type_);
const char* GetEntTypeName(EntType type_);
void PopulateEntSpecsTable();
void SetEntAs(Ent& ent, EntType type_);
AFrameId GetDefaultAnimIdForSpec(EntType type_);

} // namespace splonks

#pragma once

#include "effects/effect_id.hpp"
#include "aframe_id.hpp"
#include "hud/types.hpp"
#include "math_types.hpp"
#include "sim/fxp.hpp"
#include "utils.hpp"
#include "vid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct SDL_Renderer;

namespace splonks {

struct Audio;
struct Ent;
struct Graphics;
struct State;

constexpr std::size_t kMaxEntEffects = 12;

enum class EffectUiKind : std::uint8_t {
    Hidden,
    Passive,
    Temporary,
};

enum class EffectModifierTarget : std::uint8_t {
    // Multiplies gravity acceleration. Base: 1.0.
    GravityScale,
    // Multiplies velocity after physics integration. Base: 1.0.
    VelocityDampingX,
    VelocityDampingY,
    // Multiplies controller-chosen horizontal top speeds. Base: 1.0.
    MoveSpeedScale,
    // Clamps positive/downward velocity. Base: ent/controller max fall speed.
    MaxFallSpeed,
    // Upward acceleration applied from fluid-like effects, scaled by ent buoyancy. Base: 0.0.
    BuoyancyStrength,
    // Multiplies fall danger accumulation. Override 0 disables fall damage buildup. Base: 1.0.
    FallTimerRate,
    HiddenTreasureVisibility,
    JumpImpulse,
    SpikeDamageTaken,
    StompDamage,
    // Multiplies final stomp damage. Override 0 disables stomp attempts. Base: 1.0.
    StompDamageScale,
    StompBounceImpulse,
    ThrowHorizontalBoost,
    // Enables a controller/common helper to swim upward on jump. Base: 0.0.
    SwimImpulse,
};

enum class EffectModifierOp : std::uint8_t {
    Add,
    Multiply,
    Override,
    Min,
    Max,
};

enum class EffectHookType : std::uint8_t {
    Throw,
    Death,
    Grounded,
    BlockingContact,
};

struct EffectInstance {
    EffectId id = EffectId::None;
    std::int32_t count = 0;
    sim::Scalar value = sim::Scalar::zero();
    std::uint32_t frames_remaining = 0;
};

struct EntEffects {
    std::array<EffectInstance, kMaxEntEffects> effects{};
    std::uint8_t count = 0;
};

struct BoxedEntEffects {
    std::unique_ptr<EntEffects> value;

    BoxedEntEffects() = default;
    BoxedEntEffects(const BoxedEntEffects& other);
    BoxedEntEffects& operator=(const BoxedEntEffects& other);
    BoxedEntEffects(BoxedEntEffects&& other) noexcept = default;
    BoxedEntEffects& operator=(BoxedEntEffects&& other) noexcept = default;

    EntEffects* get() {
        return value.get();
    }

    const EntEffects* get() const {
        return value.get();
    }

    EntEffects& emplace() {
        value = std::make_unique<EntEffects>();
        return *value;
    }

    void reset() {
        value.reset();
    }

    EntEffects* operator->() {
        return value.get();
    }

    const EntEffects* operator->() const {
        return value.get();
    }
};

struct EffectModifier {
    EffectModifierTarget target = EffectModifierTarget::GravityScale;
    EffectModifierOp op = EffectModifierOp::Add;
    sim::Scalar value = sim::Scalar::zero();
};

struct EffectHookContext {
    EffectHookType type = EffectHookType::Throw;
    std::optional<VID> actor_vid = std::nullopt;
    std::optional<VID> target_vid = std::nullopt;
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
};

using EffectHookHandler = void (*)(Ent& owner, EffectInstance& effect, State& state, Audio* audio, const EffectHookContext& hook);
using EffectExpiryPredicate = bool (*)(const Ent& owner, const EffectInstance& effect, const State& state, const EffectHookContext& hook);
using EffectHudCountTextFn = std::optional<std::string> (*)(const EffectInstance& effect);
using EffectWorldOverlayFn = void (*)(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Ent& owner,
    const EffectInstance& effect
);

struct EffectSpec {
    EffectId id = EffectId::None;
    const char* debug_name = "None";
    AFrameId icon_anim_id = kInvalidAFrameId;
    EffectUiKind ui_kind = EffectUiKind::Hidden;
    std::int32_t default_count = 0;
    EffectHudCountTextFn hud_count_text = nullptr;
    HudAnchor hud_count_anchor = HudAnchor::BottomRight;
    EffectWorldOverlayFn render_world_overlay = nullptr;
    std::vector<EffectModifier> modifiers;
    EffectHookHandler on_hook = nullptr;
    EffectExpiryPredicate should_expire = nullptr;
};

const EffectSpec& GetEffectSpec(EffectId id);
const char* EffectIdToString(EffectId id);
EffectInstance* FindEffect(Ent& ent, EffectId id);
const EffectInstance* FindEffect(const Ent& ent, EffectId id);
bool HasEffect(const Ent& ent, EffectId id);
EffectInstance* AddEffect(Ent& ent, EffectId id, std::int32_t count = 0, std::uint32_t frames_remaining = 0);
void RemoveEffect(Ent& ent, EffectId id);
void SetEffect(Ent& ent, EffectId id, bool enabled);
void StepEffectTimers(Ent& ent);
float GetModifiedEffectValue(
    const Ent& ent,
    EffectModifierTarget target,
    float base_value,
    const State* state = nullptr
);
void ApplyEffectHookToEnt(Ent& ent, State& state, Audio* audio, const EffectHookContext& hook);
void ApplyEffectHookToAll(State& state, Audio* audio, const EffectHookContext& hook);

} // namespace splonks

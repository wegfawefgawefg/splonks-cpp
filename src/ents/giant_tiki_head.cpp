#include "ents/giant_tiki_head.hpp"

#include "audio_emitters.hpp"
#include "audio.hpp"
#include "ent/spec.hpp"
#include "aframe_id.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

namespace splonks::ents::giant_tiki_head {

namespace {

constexpr float kBoulderReleaseDelayFrames = 60.0F;
constexpr float kTikiHeadWindupShakeIntervalFrames = 6.0F;
constexpr float kTikiHeadWindupShakeForegroundAmount = 0.34F;
constexpr float kTikiHeadWindupShakeBackgroundAmount = 0.24F;
constexpr float kTikiHeadWindupShakeEntAmount = 0.32F;
constexpr float kTikiHeadWindupShakeRadiusTiles = 2.3F;
constexpr float kTikiHeadReleaseShakeForegroundAmount = 0.95F;
constexpr float kTikiHeadReleaseShakeBackgroundAmount = 0.72F;
constexpr float kTikiHeadReleaseShakeEntAmount = 0.90F;
constexpr float kTikiHeadReleaseShakeRadiusTiles = 3.0F;

void AddTikiHeadWindupShake(State& state, const Ent& head) {
    AddShake(
        state,
        head.GetRenderCenter(),
        kTikiHeadWindupShakeForegroundAmount,
        kTikiHeadWindupShakeBackgroundAmount,
        kTikiHeadWindupShakeEntAmount,
        kTikiHeadWindupShakeRadiusTiles
    );
}

void AddTikiHeadReleaseShake(State& state, const Ent& head) {
    AddShake(
        state,
        head.GetRenderCenter(),
        kTikiHeadReleaseShakeForegroundAmount,
        kTikiHeadReleaseShakeBackgroundAmount,
        kTikiHeadReleaseShakeEntAmount,
        kTikiHeadReleaseShakeRadiusTiles
    );
}

const Ent* FindClosestPlayerToHead(const Ent& head, const State& state) {
    const Ent* best_player = nullptr;
    sim::Scalar best_distance_sq{};
    const sim::FxVec2 head_center = head.GetSimCenter();
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
            continue;
        }

        const sim::FxVec2 delta =
            GetNearestWorldDelta(state.stage, head_center, player->GetSimCenter());
        const sim::Scalar distance_sq = gfxp::length_sq(delta);
        if (best_player == nullptr || distance_sq < best_distance_sq) {
            best_player = player;
            best_distance_sq = distance_sq;
        }
    }
    return best_player;
}

std::optional<VID> SpawnBoulderForHead(Ent& head, State& state, Audio& audio) {
    (void)audio;

    Ent* const boulder = world_ops::SpawnEnt(state, EntType::Boulder, [&](Ent& spawned_boulder) {
        spawned_boulder.SetSimCenter(head.GetSimCenter());

        const Ent* const player = FindClosestPlayerToHead(head, state);
        if (player != nullptr) {
            const sim::FxVec2 delta =
                GetNearestWorldDelta(state.stage, head.GetSimCenter(), player->GetSimCenter());
            spawned_boulder.facing = delta.x < sim::Scalar::zero() ? Side::Left : Side::Right;
        } else {
            spawned_boulder.facing = Side::Right;
        }
    }, std::nullopt);
    if (boulder == nullptr) {
        return std::nullopt;
    }

    AddTikiHeadReleaseShake(state, head);
    (void)PlayWorldSoundEmitter(state, head.GetRenderCenter(), audio_asset_ids::BoulderHitGround);
    return boulder->vid;
}

} // namespace

extern const EntSpec kGiantTikiHeadSpec{
    .type_ = EntType::GiantTikiHead,
    .size = EntSpecSize(32.0F, 32.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_stomp = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .step_logic = StepEntLogicAsGiantTikiHead,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GiantTikiHead),
};

void StepEntLogicAsGiantTikiHead(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Ent& head = state.ents.ents[ent_idx];

    if (!head.ent_a.has_value()) {
        return;
    }

    const Ent* const idol = state.ents.GetEnt(*head.ent_a);
    if (idol == nullptr || !idol->active) {
        return;
    }

    if (head.ai_state == EntAiState::Idle) {
        if (sim::ToPixelIVec2Round(idol->pos) == head.point_a) {
            return;
        }

        head.ai_state = EntAiState::Disturbed;
        head.counter_a = ToFxScalar(kBoulderReleaseDelayFrames);
        SetAnim(head, HashAFrameIdConstexpr("giant_tiki_head_hole"));
        (void)PlayAttachedSoundEmitter(
            state,
            head.vid,
            FVec2::New(0.0F, 0.0F),
            audio_asset_ids::BoulderLatch
        );
        return;
    }

    if (head.ent_b.has_value()) {
        return;
    }

    if (head.counter_a > sim::Scalar::zero()) {
        head.counter_a -= sim::Scalar::from_int(1);
        if (head.counter_a < sim::Scalar::zero()) {
            head.counter_a = sim::Scalar::zero();
        }
        const int shake_interval = static_cast<int>(kTikiHeadWindupShakeIntervalFrames);
        if (shake_interval > 0 &&
            head.counter_a.trunc_int() % shake_interval == 0) {
            AddTikiHeadWindupShake(state, head);
        }
        return;
    }

    head.ent_b = SpawnBoulderForHead(head, state, audio);
}

} // namespace splonks::ents::giant_tiki_head

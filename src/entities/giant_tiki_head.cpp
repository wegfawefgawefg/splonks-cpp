#include "entities/giant_tiki_head.hpp"

#include "audio_emitters.hpp"
#include "audio.hpp"
#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "gameplay_authority.hpp"
#include "gameplay_events.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <limits>

namespace splonks::entities::giant_tiki_head {

namespace {

constexpr float kBoulderReleaseDelayFrames = 60.0F;
constexpr float kTikiHeadWindupShakeIntervalFrames = 6.0F;
constexpr float kTikiHeadWindupShakeForegroundAmount = 0.34F;
constexpr float kTikiHeadWindupShakeBackgroundAmount = 0.24F;
constexpr float kTikiHeadWindupShakeEntityAmount = 0.32F;
constexpr float kTikiHeadWindupShakeRadiusTiles = 2.3F;
constexpr float kTikiHeadReleaseShakeForegroundAmount = 0.95F;
constexpr float kTikiHeadReleaseShakeBackgroundAmount = 0.72F;
constexpr float kTikiHeadReleaseShakeEntityAmount = 0.90F;
constexpr float kTikiHeadReleaseShakeRadiusTiles = 3.0F;

void AddTikiHeadWindupShake(State& state, const Entity& head) {
    AddShake(
        state,
        head.GetCenter(),
        kTikiHeadWindupShakeForegroundAmount,
        kTikiHeadWindupShakeBackgroundAmount,
        kTikiHeadWindupShakeEntityAmount,
        kTikiHeadWindupShakeRadiusTiles
    );
}

void AddTikiHeadReleaseShake(State& state, const Entity& head) {
    AddShake(
        state,
        head.GetCenter(),
        kTikiHeadReleaseShakeForegroundAmount,
        kTikiHeadReleaseShakeBackgroundAmount,
        kTikiHeadReleaseShakeEntityAmount,
        kTikiHeadReleaseShakeRadiusTiles
    );
}

const Entity* FindClosestPlayerToHead(const Entity& head, const State& state) {
    const Entity* best_player = nullptr;
    float best_distance_sq = std::numeric_limits<float>::max();
    const Vec2 head_center = head.GetCenter();
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }
        const Entity* const player = state.entity_manager.GetEntity(*slot.entity_vid);
        if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
            continue;
        }

        const Vec2 delta = GetNearestWorldDelta(state.stage, head_center, player->GetCenter());
        const float distance_sq = delta.x * delta.x + delta.y * delta.y;
        if (best_player == nullptr || distance_sq < best_distance_sq) {
            best_player = player;
            best_distance_sq = distance_sq;
        }
    }
    return best_player;
}

std::optional<VID> SpawnBoulderForHead(Entity& head, State& state, Audio& audio) {
    (void)audio;
    if (!HasLocalGameplayAuthorityForEntity(state, head.vid)) {
        return std::nullopt;
    }

    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Entity* const boulder = state.entity_manager.GetEntityMut(*vid);
    if (boulder == nullptr) {
        return std::nullopt;
    }

    SetEntityAs(*boulder, EntityType::Boulder);
    boulder->SetCenter(head.GetCenter());

    const Entity* const player = FindClosestPlayerToHead(head, state);
    if (player != nullptr) {
        const Vec2 delta = GetNearestWorldDelta(state.stage, head.GetCenter(), player->GetCenter());
        boulder->facing = delta.x < 0.0F ? LeftOrRight::Left : LeftOrRight::Right;
    } else {
        boulder->facing = LeftOrRight::Right;
    }
    EmitEntitySpawnedGameplayEvent(state, *boulder, std::nullopt);

    AddTikiHeadReleaseShake(state, head);
    (void)PlayWorldSoundEmitter(state, head.GetCenter(), audio_asset_ids::BoulderHitGround);
    return vid;
}

} // namespace

extern const EntityArchetype kGiantTikiHeadArchetype{
    .type_ = EntityType::GiantTikiHead,
    .size = Vec2::New(32.0F, 32.0F),
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsGiantTikiHead,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GiantTikiHead),
};

void StepEntityLogicAsGiantTikiHead(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& head = state.entity_manager.entities[entity_idx];

    if (!head.entity_a.has_value()) {
        return;
    }

    const Entity* const idol = state.entity_manager.GetEntity(*head.entity_a);
    if (idol == nullptr || !idol->active) {
        return;
    }

    if (head.ai_state == EntityAiState::Idle) {
        if (ToIVec2(idol->pos) == head.point_a) {
            return;
        }

        head.ai_state = EntityAiState::Disturbed;
        head.counter_a = kBoulderReleaseDelayFrames;
        SetAnimation(head, HashFrameDataIdConstexpr("giant_tiki_head_hole"));
        (void)PlayAttachedSoundEmitter(
            state,
            head.vid,
            Vec2::New(0.0F, 0.0F),
            audio_asset_ids::BoulderLatch
        );
        return;
    }

    if (head.entity_b.has_value()) {
        return;
    }

    if (head.counter_a > 0.0F) {
        head.counter_a -= 1.0F;
        if (head.counter_a < 0.0F) {
            head.counter_a = 0.0F;
        }
        const int shake_interval = static_cast<int>(kTikiHeadWindupShakeIntervalFrames);
        if (shake_interval > 0 &&
            static_cast<int>(head.counter_a) % shake_interval == 0) {
            AddTikiHeadWindupShake(state, head);
        }
        return;
    }

    head.entity_b = SpawnBoulderForHead(head, state, audio);
}

} // namespace splonks::entities::giant_tiki_head

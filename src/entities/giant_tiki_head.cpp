#include "entities/giant_tiki_head.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

namespace splonks::entities::giant_tiki_head {

namespace {

constexpr float kBoulderReleaseDelayFrames = 60.0F;

std::optional<VID> SpawnBoulderForHead(Entity& head, State& state, Audio& audio) {
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

    const Entity* const player =
        state.player_vid.has_value() ? state.entity_manager.GetEntity(*state.player_vid) : nullptr;
    if (player != nullptr) {
        const Vec2 delta = player->GetCenter() - head.GetCenter();
        boulder->facing = delta.x < 0.0F ? LeftOrRight::Left : LeftOrRight::Right;
    } else {
        boulder->facing = LeftOrRight::Right;
    }

    audio.PlaySoundEffect(SoundEffect::BoulderHitGround);
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
    .draw_layer = DrawLayer::Middle,
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
        audio.PlaySoundEffect(SoundEffect::BoulderLatch);
        return;
    }

    if (head.entity_b.has_value()) {
        return;
    }

    if (head.counter_a > 0.0F) {
        head.counter_a -= 1.0F;
        return;
    }

    head.entity_b = SpawnBoulderForHead(head, state, audio);
}

} // namespace splonks::entities::giant_tiki_head

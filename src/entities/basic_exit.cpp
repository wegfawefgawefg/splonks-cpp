#include "entities/basic_exit.hpp"

#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "stage_progression.hpp"
#include "world_query.hpp"

#include <limits>

namespace splonks::entities::basic_exit {

namespace {

Vec2 GetAabbCenter(const AABB& aabb) {
    return (aabb.tl + aabb.br) / 2.0F;
}

float GetDistanceSq(const Vec2& from, const Vec2& to, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, from, to);
    return (delta.x * delta.x) + (delta.y * delta.y);
}

const Entity* GetActiveBasicExitEntity(
    std::size_t entity_idx,
    const State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return nullptr;
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.active || entity.type_ != EntityType::BasicExit) {
        return nullptr;
    }
    return &entity;
}

std::optional<ExitPrompt> BuildExitPromptForEntity(
    std::size_t entity_idx,
    const State& state,
    const Graphics& graphics
) {
    const Entity* const exit_entity = GetActiveBasicExitEntity(entity_idx, state);
    if (exit_entity == nullptr) {
        return std::nullopt;
    }

    if (!state.player_vid.has_value()) {
        return std::nullopt;
    }
    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active) {
        return std::nullopt;
    }

    const std::optional<std::size_t> overlapping_exit_idx =
        FindOverlappingBasicExitEntityIdx(*player, state, graphics);
    if (!overlapping_exit_idx.has_value() || *overlapping_exit_idx != entity_idx) {
        return std::nullopt;
    }

    return ExitPrompt{
        .entity_idx = entity_idx,
        .action_text = "RB",
        .message_text = "",
        .show_down_arrow = true,
        .allowed = true,
    };
}

void QueueDefaultExitTransition(State& state) {
    switch (state.stage.stage_type) {
    case StageType::Test1:
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Test1), true);
        break;
    case StageType::SplkMines1:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::SplkMines2), true);
        break;
    case StageType::SplkMines2:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::SplkMines3), true);
        break;
    case StageType::SplkMines3:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Ice1), true);
        break;
    case StageType::Ice1:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Ice2), true);
        break;
    case StageType::Ice2:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Ice3), true);
        break;
    case StageType::Ice3:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Desert1), true);
        break;
    case StageType::Desert1:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Desert2), true);
        break;
    case StageType::Desert2:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Desert3), true);
        break;
    case StageType::Desert3:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Temple1), true);
        break;
    case StageType::Temple1:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Temple2), true);
        break;
    case StageType::Temple2:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Temple3), true);
        break;
    case StageType::Temple3:
        state.depth += 1;
        QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Boss), true);
        break;
    case StageType::Boss:
        state.stage = Stage::NewBlank();
        state.mode = Mode::Win;
        break;
    case StageType::Blank:
        state.stage = Stage::NewBlank();
        state.mode = Mode::Win;
        break;
    }
}

bool TryTakeExit(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const std::optional<ExitPrompt> prompt = BuildExitPromptForEntity(entity_idx, state, graphics);
    if (!prompt.has_value() || !prompt->allowed) {
        return false;
    }

    const Entity& exit_entity = state.entity_manager.entities[entity_idx];
    audio.PlaySoundEffect(SoundEffect::StageWin);
    if (exit_entity.transition_target.has_value()) {
        QueueStageTransition(state, *exit_entity.transition_target);
    } else {
        QueueDefaultExitTransition(state);
    }
    return true;
}

} // namespace

std::optional<std::size_t> FindOverlappingBasicExitEntityIdx(
    const Entity& entity,
    const State& state,
    const Graphics& graphics
) {
    if (!entity.active) {
        return std::nullopt;
    }

    const AABB entity_aabb = common::GetContactAabbForEntity(entity, graphics);
    const Vec2 entity_center = GetAabbCenter(entity_aabb);
    const std::vector<VID> results = QueryEntitiesInAabb(state, entity_aabb, entity.vid);

    float best_distance_sq = std::numeric_limits<float>::max();
    std::optional<std::size_t> best_entity_idx;
    for (const VID& vid : results) {
        const Entity* const other = state.entity_manager.GetEntity(vid);
        if (other == nullptr || !other->active || other->type_ != EntityType::BasicExit) {
            continue;
        }

        const AABB other_aabb = GetNearestWorldAabb(
            state.stage,
            entity_center,
            common::GetContactAabbForEntity(*other, graphics)
        );
        if (!AabbsIntersect(entity_aabb, other_aabb)) {
            continue;
        }

        const float distance_sq = GetDistanceSq(entity_center, GetAabbCenter(other_aabb), state.stage);
        if (!best_entity_idx.has_value() || distance_sq < best_distance_sq) {
            best_distance_sq = distance_sq;
            best_entity_idx = vid.id;
        }
    }

    return best_entity_idx;
}

std::optional<std::size_t> FindOverlappingBasicExitEntityIdxForPlayer(
    const State& state,
    const Graphics& graphics
) {
    if (!state.player_vid.has_value()) {
        return std::nullopt;
    }
    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr) {
        return std::nullopt;
    }
    return FindOverlappingBasicExitEntityIdx(*player, state, graphics);
}

bool IsEntityTouchingBasicExit(
    const Entity& entity,
    const State& state,
    const Graphics& graphics
) {
    return FindOverlappingBasicExitEntityIdx(entity, state, graphics).has_value();
}

void StepEntityLogicAsBasicExit(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;

    if (state.mode != Mode::Playing) {
        return;
    }

    const std::optional<ExitPrompt> prompt = BuildExitPromptForEntity(entity_idx, state, graphics);
    if (prompt.has_value()) {
        if (state.player_vid.has_value()) {
            state.ClaimInteractForEntity(*state.player_vid);
        }
        const Entity& exit_entity = state.entity_manager.entities[entity_idx];
        const Entity* const player = state.player_vid.has_value() ? state.entity_manager.GetEntity(*state.player_vid) : nullptr;
        if (player != nullptr) {
            const AABB player_aabb = common::GetContactAabbForEntity(*player, graphics);
            const AABB nearest_exit_aabb = GetNearestWorldAabb(
                state.stage,
                GetAabbCenter(player_aabb),
                common::GetContactAabbForEntity(exit_entity, graphics)
            );
            state.AddWorldPrompt(WorldPrompt{
                .world_pos = Vec2::New((nearest_exit_aabb.tl.x + nearest_exit_aabb.br.x) * 0.5F, nearest_exit_aabb.tl.y - 6.0F),
                .action_text = prompt->action_text,
                .message_text = prompt->message_text,
                .show_down_arrow = prompt->show_down_arrow,
                .quantity = 0,
                .icon_animation_id = std::nullopt,
            });
        }
    }

    if (state.pending_stage_transition.has_value() || !state.playing_inputs.equip_button.pressed) {
        return;
    }

    (void)TryTakeExit(entity_idx, state, graphics, audio);
}

extern const EntityArchetype kBasicExitArchetype{
    .type_ = EntityType::BasicExit,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .draw_layer = DrawLayer::Background,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsBasicExit,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Exit),
};

} // namespace splonks::entities::basic_exit

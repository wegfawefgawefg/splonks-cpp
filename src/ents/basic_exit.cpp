#include "ents/basic_exit.hpp"

#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "stage_progression.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <cstdint>
#include <limits>

namespace splonks::ents::basic_exit {

namespace {

std::int64_t GetDistanceSq(sim::FxVec2 from, sim::FxVec2 to, const Stage& stage) {
    const sim::FxVec2 delta = GetNearestWorldDelta(stage, from, to);
    const std::int64_t dx = delta.x.raw_value();
    const std::int64_t dy = delta.y.raw_value();
    return (dx * dx) + (dy * dy);
}

const Ent* GetActiveBasicExitEnt(
    std::size_t ent_idx,
    const State& state
) {
    if (ent_idx >= state.ents.ents.size()) {
        return nullptr;
    }

    const Ent& ent = state.ents.ents[ent_idx];
    if (!ent.active || ent.type_ != EntType::BasicExit) {
        return nullptr;
    }
    return &ent;
}

std::optional<ExitPrompt> BuildExitPromptForEnt(
    std::size_t ent_idx,
    const State& state,
    const Graphics& graphics,
    const Ent& player
) {
    const Ent* const exit_ent = GetActiveBasicExitEnt(ent_idx, state);
    if (exit_ent == nullptr) {
        return std::nullopt;
    }

    if (!player.active) {
        return std::nullopt;
    }

    const std::optional<std::size_t> overlapping_exit_idx =
        FindOverlappingBasicExitEntIdx(player, state, graphics);
    if (!overlapping_exit_idx.has_value() || *overlapping_exit_idx != ent_idx) {
        return std::nullopt;
    }

    const bool allowed = IsStageExitAllowed(state, exit_ent->stage_exit_id);

    return ExitPrompt{
        .ent_idx = ent_idx,
        .action_text = "RB",
        .message_text = allowed ? "" : "locked",
        .show_down_arrow = true,
        .allowed = allowed,
    };
}

} // namespace

std::optional<std::size_t> FindOverlappingBasicExitEntIdx(
    const Ent& ent,
    const State& state,
    const Graphics& graphics
) {
    if (!ent.active) {
        return std::nullopt;
    }

    const sim::FxAABB ent_aabb = common::GetContactAabbForEnt(ent, graphics);
    const sim::FxVec2 ent_center = ent_aabb.center();
    const std::vector<VID> results = QueryEntsInAabb(state, ent_aabb, ent.vid);

    std::int64_t best_distance_sq = std::numeric_limits<std::int64_t>::max();
    std::optional<std::size_t> best_ent_idx;
    for (const VID& vid : results) {
        const Ent* const other = state.ents.GetEnt(vid);
        if (other == nullptr || !other->active || other->type_ != EntType::BasicExit) {
            continue;
        }

        const sim::FxAABB other_aabb = GetNearestWorldAabb(
            state.stage,
            ent_center,
            common::GetContactAabbForEnt(*other, graphics)
        );
        if (!gfxp::aabbs_intersect(ent_aabb, other_aabb)) {
            continue;
        }

        const std::int64_t distance_sq = GetDistanceSq(ent_center, other_aabb.center(), state.stage);
        if (!best_ent_idx.has_value() || distance_sq < best_distance_sq) {
            best_distance_sq = distance_sq;
            best_ent_idx = vid.id;
        }
    }

    return best_ent_idx;
}

bool IsEntTouchingBasicExit(
    const Ent& ent,
    const State& state,
    const Graphics& graphics
) {
    return FindOverlappingBasicExitEntIdx(ent, state, graphics).has_value();
}

void StepEntLogicAsBasicExit(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;

    if (state.mode != Mode::Playing) {
        return;
    }

    const Ent& exit_ent = state.ents.ents[ent_idx];
    for (const PlayerSlot& slot : state.players.slots) {
        if (!ShouldSimulatePlayerSlotGameplay(state, slot)) {
            continue;
        }

        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr) {
            continue;
        }

        const std::optional<ExitPrompt> player_prompt =
            BuildExitPromptForEnt(ent_idx, state, graphics, *player);
        if (!player_prompt.has_value()) {
            continue;
        }

        state.ClaimInteractForEnt(*slot.ent_vid);
        const sim::FxAABB player_aabb = common::GetContactAabbForEnt(*player, graphics);
        const sim::FxVec2 player_center = player_aabb.center();
        const sim::FxAABB nearest_exit_aabb = GetNearestWorldAabb(
            state.stage,
            player_center,
            common::GetContactAabbForEnt(exit_ent, graphics)
        );
        const sim::FxVec2 prompt_base = GetNearestWorldPoint(
            state.stage,
            player_center,
            sim::FxVec2{(nearest_exit_aabb.tl.x + nearest_exit_aabb.br.x) / 2,
                      nearest_exit_aabb.tl.y}
        );
        state.AddWorldPrompt(WorldPrompt{
            .world_pos = sim::ToRenderVec2(prompt_base + sim::PixelVec2(0, -6)),
            .action_text = player_prompt->action_text,
            .message_text = player_prompt->message_text,
            .show_down_arrow = player_prompt->show_down_arrow,
            .quantity = 0,
            .icon_anim_id = std::nullopt,
        });

        if (state.pending_stage_transition.has_value() ||
            !slot.inputs.equip_button.pressed ||
            !player_prompt->allowed) {
            continue;
        }

        (void)world_ops::TryApplyInteractEnt(player->vid, exit_ent.vid, state, graphics, audio);
        return;
    }
}

bool OnInteractAsBasicExit(
    std::size_t ent_idx,
    std::size_t interactor_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    const Ent* const exit_ent = GetActiveBasicExitEnt(ent_idx, state);
    if (exit_ent == nullptr ||
        interactor_idx >= state.ents.ents.size() ||
        state.pending_stage_transition.has_value()) {
        return false;
    }

    const Ent& interactor = state.ents.ents[interactor_idx];
    if (!interactor.active ||
        !IsEntTouchingBasicExit(interactor, state, graphics) ||
        !IsStageExitAllowed(state, exit_ent->stage_exit_id)) {
        return false;
    }

    (void)PlayEntCenterSoundEmitter(state, *exit_ent, audio_asset_ids::StageWin);
    if (exit_ent->transition_target.has_value()) {
        QueueStageTransition(state, *exit_ent->transition_target);
    } else {
        QueueStageExitTransition(state, exit_ent->stage_exit_id);
    }
    return true;
}

extern const EntSpec kBasicExitSpec{
    .type_ = EntType::BasicExit,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .on_interact = OnInteractAsBasicExit,
    .step_logic = StepEntLogicAsBasicExit,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Exit),
};

} // namespace splonks::ents::basic_exit

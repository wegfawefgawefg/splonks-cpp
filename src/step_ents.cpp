#include "step_ents.hpp"

#include "ents/common/common.hpp"
#include "controls.hpp"
#include "ent.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "ent/manager.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace splonks {

namespace {

bool HasUseActivity(const Ent& ent) {
    return ent.use_state.down || ent.use_state.pressed || ent.use_state.released;
}

bool HasAreaCallbacks(const Ent& ent) {
    return ent.on_area_enter != nullptr || ent.on_area_exit != nullptr;
}

std::vector<VID> GetAreaOverlapVids(std::size_t area_idx, const State& state) {
    const Ent& area_ent = state.ents.ents[area_idx];
    const FxAABB area = area_ent.GetAABB();

    std::vector<VID> overlaps;
    for (const VID& vid : QueryEntsInAabb(state, area, area_ent.vid)) {
        const Ent* const other = state.ents.GetEnt(vid);
        if (other == nullptr || !other->active) {
            continue;
        }
        if (!WorldAabbsIntersect(state.stage, area, other->GetAABB())) {
            continue;
        }
        overlaps.push_back(vid);
    }
    return overlaps;
}

bool ContainsVid(const std::vector<VID>& vids, VID vid) {
    return std::find(vids.begin(), vids.end(), vid) != vids.end();
}

void StepAreaEntOverlaps(State& state, Graphics& graphics, Audio& audio) {
    for (const VID& area_vid : state.area_listener_vids) {
        const Ent* const area_ent_ptr = state.ents.GetEnt(area_vid);
        if (area_ent_ptr == nullptr) {
            continue;
        }

        const std::size_t area_idx = area_vid.id;
        const Ent& area_ent = *area_ent_ptr;
        if (!area_ent.active || !HasAreaCallbacks(area_ent)) {
            continue;
        }
        const std::vector<VID> previous_overlaps = area_ent.inside_vids.value_or(std::vector<VID>{});
        const std::vector<VID> current_overlaps = GetAreaOverlapVids(area_idx, state);

        for (const VID& vid : previous_overlaps) {
            if (ContainsVid(current_overlaps, vid)) {
                continue;
            }
            Ent* const area_mut = state.ents.GetEntMut(area_ent.vid);
            if (area_mut == nullptr || !area_mut->active || area_mut->on_area_exit == nullptr) {
                continue;
            }
            if (state.ents.GetEnt(vid) == nullptr) {
                continue;
            }
            area_mut->on_area_exit(area_idx, vid.id, state, graphics, audio);
        }

        for (const VID& vid : current_overlaps) {
            if (ContainsVid(previous_overlaps, vid)) {
                continue;
            }
            Ent* const area_mut = state.ents.GetEntMut(area_ent.vid);
            if (area_mut == nullptr || !area_mut->active || area_mut->on_area_enter == nullptr) {
                continue;
            }
            if (state.ents.GetEnt(vid) == nullptr) {
                continue;
            }
            area_mut->on_area_enter(area_idx, vid.id, state, graphics, audio);
        }

        Ent* const area_mut = state.ents.GetEntMut(area_ent.vid);
        if (area_mut == nullptr || !area_mut->active) {
            continue;
        }
        area_mut->inside_vids = current_overlaps;
    }
}

void ApplyStageWrapAndVoidDeath(std::size_t ent_idx, State& state, Audio& audio) {
    Ent& ent = state.ents.ents[ent_idx];
    state.stage.NormalizeEntPositionForWrap(ent);

    if (!state.stage.HasVoidDeathY()) {
        return;
    }
    if (ent.health == 0 || ent.condition == EntCondition::Dead) {
        return;
    }

    if (ent.GetAABB().br.y <= state.stage.GetVoidDeathY()) {
        return;
    }

    ent.vel = FxVec2::zero();
    ent.health = 0;
    ents::common::DieIfDead(ent_idx, state, audio);
}

void ClearUseEdgesAfterFrame(Ent& ent) {
    ent.use_state.pressed = false;
    if (!ent.use_state.down) {
        ent.use_state.released = false;
        ent.use_state.frames = 0;
        ent.use_state.user_vid.reset();
        ent.use_state.source = AttachMode::None;
    }
}

} // namespace

/** Step the logic of ents, followed by their physics.  */
/*  Stepping Ents:
        Your ent must implement the following:
            step_logic_<ent_name>(game, map):
                - for logic
                - state machine stuff
                - other ent following
                - actions
            step_physics_<ent_name>(base_ent, game, map):
                - for motion
            step_anim_<ent_name>(base ent, game, map):
                - for stepping the anim state machine basically (can have common impls too)
        If several of your ents end up internally sharing a mutual implementation of some feature,
        you dont need a match out here or in step, just use the function which allows for that feature internally.
        compiler-san will make sure you have the necessary fields if you pass yourself into say,
            flying_ents_physics_step(*self)). because if you dont, compiler: reee
        You may also need to check if an ent has some trait, and that can go in ent, and instead of having a table you can just
        force every ent to implement is_burnable() for example. and the dyn table will take care of that for you.
*/
namespace {

void StepOneEnt(std::size_t ent_idx, State& state, Audio& audio, Graphics& graphics, float dt) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    if (!state.ents.ents[ent_idx].active) {
        return;
    }

    ClearTransientMovementFlags(state.ents.ents[ent_idx]);
    ents::common::CommonStep(ent_idx, state, graphics, audio, dt);
    if (!state.ents.ents[ent_idx].active) {
        return;
    }

    const Ent& current_ent = state.ents.ents[ent_idx];
    if (HasUseActivity(current_ent) && current_ent.on_use != nullptr) {
        current_ent.on_use(ent_idx, state, graphics, audio);
    }
    if (state.ents.ents[ent_idx].active && current_ent.step_logic != nullptr) {
        current_ent.step_logic(ent_idx, state, graphics, audio, dt);
    }
    if (!state.ents.ents[ent_idx].active) {
        return;
    }

    ents::common::CommonPostStep(ent_idx, state, graphics, audio, dt);
    if (!state.ents.ents[ent_idx].active) {
        return;
    }

    if (state.ents.ents[ent_idx].has_physics) {
        if (current_ent.step_physics != nullptr) {
            current_ent.step_physics(ent_idx, state, graphics, audio, dt);
        } else {
            ents::common::StepStandardPhysics(ent_idx, state, graphics, audio, dt);
        }
        if (state.ents.ents[ent_idx].active) {
            ApplyStageWrapAndVoidDeath(ent_idx, state, audio);
        }
    }

    ents::common::ApplyDeactivateConditions(ent_idx, state);
    state.UpdateSidForEnt(ent_idx, graphics);
    Ent& mutable_ent = state.ents.ents[ent_idx];
    ClearUseEdgesAfterFrame(mutable_ent);
    mutable_ent.last_condition = mutable_ent.condition;
    mutable_ent.last_ai_state = mutable_ent.ai_state;
}

bool IsPlayerSlotEnt(const State& state, const Ent& ent) {
    return state.players.FindPlayerIdForEnt(ent.vid).has_value();
}

bool ShouldRunFullLocalStepForNonPlayerEnt(const State& state, const Ent& ent) {
    (void)state;
    (void)ent;
    return true;
}

bool ShouldRunFullPlayerSlotStep(const State& state, const PlayerSlot& slot) {
    (void)state;
    if (!slot.ent_vid.has_value()) {
        return false;
    }
    return slot.connected;
}

void TryReleaseHeldPlayerSlotFromJump(const PlayerSlot& slot, State& state) {
    if (!slot.ent_vid.has_value()) {
        return;
    }
    Ent* const ent = state.ents.GetEntMut(*slot.ent_vid);
    if (ent == nullptr ||
        !ent->active ||
        !ent->held_by_vid.has_value() ||
        ent->attach_mode != AttachMode::Held ||
        ent->condition != EntCondition::Normal) {
        return;
    }

    const controls::ControlIntent control = controls::GetControlIntentForEnt(*ent, state);
    if (!control.jump_pressed) {
        return;
    }

    ents::common::ReleaseEntFromHolderIfAttached(*ent, state);
    ent->grounded = false;
    ent->coyote_time = 2;
}

} // namespace

void StepEnts(State& state, Audio& audio, Graphics& graphics, float dt) {
    // Step all player-controlled ents first so held items and attached
    // visuals follow the latest player positions.
    for (const PlayerSlot& slot : state.players.slots) {
        if (ShouldRunFullPlayerSlotStep(state, slot)) {
            TryReleaseHeldPlayerSlotFromJump(slot, state);
            StepOneEnt(slot.ent_vid->id, state, audio, graphics, dt);
        }
    }

    for (std::size_t ent_idx = 0; ent_idx < state.ents.ents.size(); ++ent_idx) {
        const Ent& ent = state.ents.GetEntById(ent_idx);

        if (ent.active) {
            if (IsPlayerSlotEnt(state, ent)) {
                continue;
            }
            if (ShouldRunFullLocalStepForNonPlayerEnt(state, ent)) {
                StepOneEnt(ent_idx, state, audio, graphics, dt);
            }
        }
    }

    constexpr int kAttachSyncPasses = 8;
    for (int pass = 0; pass < kAttachSyncPasses; ++pass) {
        for (std::size_t ent_idx = 0; ent_idx < state.ents.ents.size(); ++ent_idx) {
            ents::common::SyncEntAttachs(ent_idx, state, graphics);
        }
    }

    StepAreaEntOverlaps(state, graphics, audio);
}

} // namespace splonks

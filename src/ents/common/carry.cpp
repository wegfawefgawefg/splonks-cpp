#include "ents/common/common.hpp"

#include "ents/player.hpp"
#include "ent/spec.hpp"
#include "ent/spec_restore.hpp"
#include "controls.hpp"
#include "fxp.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace splonks::ents::common {

namespace {

constexpr std::uint32_t kForcedPlayerDropHarmCooldownFrames = 12;

void ApplyHeldState(Ent& ent, bool reset_anim = true) {
    ent.thrown_by.reset();
    ent.thrown_immunity_timer = 0;
    const EntSpec& spec = GetEntSpec(ent.type_);
    ent.proj_contact_damage_type = spec.proj_contact_damage_type;
    ent.proj_contact_damage_amount = spec.proj_contact_damage_amount;
    ent.proj_contact_timer = 0;
    ent.vel = FxVec2::zero();
    ent.acc = FxVec2::zero();
    RemoveEffect(ent, EffectId::NoGravityUntilContact);
    ent.rotation = FxScalar::zero();
    ent.hang_side.reset();
    ent.hang_count = 0;
    ent.climb_detach_cooldown = 0;
    ent.coyote_time = 0;
    ent.jump_hold_gravity_frames_remaining = 0;
    ent.jumped_this_frame = false;
    ent.grounded = false;
    ent.movement_flags = 0;
    if (reset_anim && ent.condition == EntCondition::Normal) {
        TrySetAnim(ent, EntDisplayState::Neutral);
    }
}

void TrackChangedEnt(std::vector<VID>& changed_ents, VID vid) {
    if (std::find(changed_ents.begin(), changed_ents.end(), vid) != changed_ents.end()) {
        return;
    }
    changed_ents.push_back(vid);
}

void RestoreDetachedCarryEnt(Ent& ent) {
    ent.held_by_vid.reset();
    ent.attach_mode = AttachMode::None;
    StopUsingEnt(ent);
    RestoreEntHasPhysicsFromSpec(ent);
    RestoreEntCanCollideFromSpec(ent);
    RestoreEntDrawLayerFromSpec(ent);
    ent.grounded = false;
    RemoveEffect(ent, EffectId::NoGravityUntilContact);
}

void SnapPlacedAttachToPixels(Ent& ent) {
    ent.pos = PixelVec2(ent.pos.x.round_int(),
                             ent.pos.y.round_int());
}

bool IsAttachDriven(const Ent& ent) {
    return ent.held_by_vid.has_value() || ent.attach_mode != AttachMode::None;
}

bool HasAttachReference(const Ent& ent, const State& state) {
    if (IsAttachDriven(ent)) {
        return true;
    }
    for (const Ent& candidate_holder : state.ents.ents) {
        if (!candidate_holder.active || candidate_holder.vid == ent.vid) {
            continue;
        }
        if ((candidate_holder.holding_vid.has_value() &&
             *candidate_holder.holding_vid == ent.vid) ||
            (candidate_holder.back_vid.has_value() &&
             *candidate_holder.back_vid == ent.vid)) {
            return true;
        }
    }
    return false;
}

bool IsPlayerEnt(const Ent& ent, const State& state) {
    return state.players.FindByEntVid(ent.vid) != nullptr;
}

bool IsVidInHolderChain(VID needle, const Ent& ent, const State& state) {
    std::optional<VID> holder_vid = ent.held_by_vid;
    constexpr int kMaxCarryChainDepth = 16;
    for (int depth = 0; depth < kMaxCarryChainDepth && holder_vid.has_value(); ++depth) {
        if (*holder_vid == needle) {
            return true;
        }
        const Ent* const holder = state.ents.GetEnt(*holder_vid);
        if (holder == nullptr || !holder->active) {
            return false;
        }
        holder_vid = holder->held_by_vid;
    }
    return false;
}

bool CanPickUpCandidate(const Ent& picker, const Ent& candidate, const State& state) {
    if (!candidate.can_be_picked_up || candidate.vid == picker.vid) {
        return false;
    }
    if (picker.back_vid.has_value() && candidate.vid == *picker.back_vid) {
        return false;
    }
    if (IsAttachDriven(candidate)) {
        return false;
    }
    if (IsVidInHolderChain(candidate.vid, picker, state)) {
        return false;
    }

    const bool dead_or_stunned =
        candidate.condition == EntCondition::Dead ||
        candidate.condition == EntCondition::Stunned;
    return !candidate.can_only_be_picked_up_if_dead_or_stunned || dead_or_stunned;
}

void SyncHeldAttachForHolder(
    const Ent& holder,
    State& state,
    const Graphics& graphics
) {
    if (!holder.holding_vid.has_value()) {
        return;
    }

    Ent* const holding = state.ents.GetEntMut(*holder.holding_vid);
    if (holding == nullptr || !holding->active) {
        return;
    }

    const EntSpec& holding_spec = GetEntSpec(holding->type_);
    const bool preserve_held_aim = holding_spec.preserve_held_aim;
    const Side aimed_facing = holding->facing;
    const FxScalar aimed_rotation = holding->rotation;

    holding->has_physics = false;
    holding->can_collide = false;
    holding->vel = FxVec2::zero();
    holding->acc = FxVec2::zero();
    holding->held_by_vid = holder.vid;
    holding->attach_mode = AttachMode::Held;
    ApplyHeldState(*holding, false);
    holding->facing = preserve_held_aim ? aimed_facing : holder.facing;
    if (preserve_held_aim) {
        holding->rotation = aimed_rotation;
    }
    holding->draw_layer = HasMovementFlag(holder, EntMovementFlag::Climbing)
                              ? DrawLayer::Background
                              : DrawLayer::Foreground;

    const FxVec2 hold_offset = FxVec2::from_pixels(4, 0);
    const FxVec2 holder_center = holder.GetCenter() + FxVec2::from_pixels(0, 1);
    const FxVec2 held_pos_target =
        holder.facing == Side::Left
            ? holder_center + FxVec2{-hold_offset.x, hold_offset.y}
            : holder_center + hold_offset;
    SetVisualCenterForEnt(*holding, graphics, held_pos_target);
    SnapPlacedAttachToPixels(*holding);
    holding->grounded = false;
    state.UpdateSidForEnt(holding->vid.id, graphics);
}

void SyncBackAttachForHolder(
    const Ent& holder,
    State& state,
    const Graphics& graphics
) {
    if (!holder.back_vid.has_value()) {
        return;
    }

    Ent* const back_item = state.ents.GetEntMut(*holder.back_vid);
    if (back_item == nullptr || !back_item->active) {
        return;
    }

    back_item->has_physics = false;
    back_item->can_collide = false;
    back_item->vel = FxVec2::zero();
    back_item->acc = FxVec2::zero();
    back_item->facing = holder.facing;
    back_item->held_by_vid = holder.vid;
    back_item->attach_mode = AttachMode::Back;

    const bool holder_climbing = HasMovementFlag(holder, EntMovementFlag::Climbing);
    const bool holder_hanging = HasMovementFlag(holder, EntMovementFlag::Hanging);

    FxVec2 back_offset{FxScalar::from_int(-3), FxScalar::zero()};
    if (holder_climbing) {
        back_offset = FxVec2{FxScalar::from_int(-2), FxScalar::zero()};
        TrySetAnim(*back_item, EntDisplayState::Climbing);
        back_item->draw_layer = DrawLayer::Foreground;
    } else if (holder_hanging) {
        back_offset = FxVec2{FxScalar::from_int(-7), FxScalar::from_int(4)};
        TrySetAnim(*back_item, EntDisplayState::Hanging);
        back_item->draw_layer = DrawLayer::Foreground;
    } else {
        back_item->draw_layer = DrawLayer::Background;
        TrySetAnim(*back_item, EntDisplayState::Neutral);
    }

    const FxVec2 holder_center = holder.GetCenter();
    const FxVec2 held_pos_target =
        holder.facing == Side::Left
            ? holder_center + FxVec2{-back_offset.x, back_offset.y}
            : holder_center + back_offset;
    back_item->SetCenter(held_pos_target);
    SnapPlacedAttachToPixels(*back_item);
    back_item->grounded = false;
    state.UpdateSidForEnt(back_item->vid.id, graphics);
}

void ApplyThrowState(
    Ent& thrower,
    Ent& thrown,
    FxVec2 throw_velocity,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    ReleaseEntFromHolder(thrown, state);

    thrown.thrown_by = thrower.vid;
    thrown.thrown_immunity_timer = kThrownByImmunityDuration;
    const EntSpec& thrown_spec = GetEntSpec(thrown.type_);
    thrown.can_apply_proj_contact = thrown_spec.can_apply_proj_contact;
    thrown.proj_contact_damage_type = thrown_spec.proj_contact_damage_type;
    thrown.proj_contact_damage_amount = thrown_spec.proj_contact_damage_amount;
    thrown.proj_contact_timer = kProjContactDuration;

    if (GetModifiedEffectValue(thrower, EffectModifierTarget::ThrowHorizontalBoost, 0.0F) != 0.0F) {
        ApplyEffectHookToEnt(
            thrower,
            state,
            &audio,
            EffectHookContext{
                .type = EffectHookType::Throw,
                .actor_vid = thrower.vid,
                .target_vid = thrown.vid,
                .world_pos = ToFVec2(thrown.GetCenter()),
            }
        );
    } else {
        RemoveEffect(thrown, EffectId::NoGravityUntilContact);
    }

    const FxVec2 thrower_center = thrower.GetCenter();
    if (thrower.size.y <= thrown.size.y) {
        const FxScalar delta = (thrown.size.y - thrower.size.y) / FxScalar::from_int(2);
        thrown.SetCenter(thrower_center - FxVec2{FxScalar::zero(), delta});
    } else {
        thrown.SetCenter(thrower_center);
    }

    if (IsPlayerEnt(thrown, state)) {
        thrown.vel = throw_velocity;
        thrown.acc = FxVec2::zero();
    } else {
        thrown.vel = FxVec2::zero();
        thrown.acc = throw_velocity;
    }
    state.UpdateSidForEnt(thrown.vid.id, graphics);
    (void)PlayEntCenterSoundEmitter(state, thrower, audio_asset_ids::Throw);
}

} // namespace

void AttachEntAsHeld(Ent& holder, Ent& held) {
    holder.holding_vid = held.vid;
    holder.holding = true;
    held.held_by_vid = holder.vid;
    held.attach_mode = AttachMode::Held;
    held.has_physics = false;
    held.can_collide = false;
    ApplyHeldState(held);
}


void ReleaseEntFromHolder(Ent& ent, State& state) {
    std::vector<VID> changed_holders;
    if (ent.held_by_vid.has_value()) {
        if (Ent* const holder = state.ents.GetEntMut(*ent.held_by_vid)) {
            bool holder_changed = false;
            if (holder->holding_vid.has_value() && *holder->holding_vid == ent.vid) {
                holder->holding_vid.reset();
                holder->holding = false;
                holder->holding_timer = kDefaultHoldingTimer;
                holder_changed = true;
            }
            if (holder->back_vid.has_value() && *holder->back_vid == ent.vid) {
                holder->back_vid.reset();
                holder_changed = true;
            }
            if (holder_changed) {
                TrackChangedEnt(changed_holders, holder->vid);
            }
        }
    }

    for (Ent& candidate_holder : state.ents.ents) {
        if (!candidate_holder.active || candidate_holder.vid == ent.vid) {
            continue;
        }
        bool holder_changed = false;
        if (candidate_holder.holding_vid.has_value() &&
            *candidate_holder.holding_vid == ent.vid) {
            candidate_holder.holding_vid.reset();
            candidate_holder.holding = false;
            candidate_holder.holding_timer = kDefaultHoldingTimer;
            holder_changed = true;
        }
        if (candidate_holder.back_vid.has_value() &&
            *candidate_holder.back_vid == ent.vid) {
            candidate_holder.back_vid.reset();
            holder_changed = true;
        }
        if (holder_changed) {
            TrackChangedEnt(changed_holders, candidate_holder.vid);
        }
    }

    ent.held_by_vid.reset();
    ent.attach_mode = AttachMode::None;
    StopUsingEnt(ent);
    RestoreEntHasPhysicsFromSpec(ent);
    RestoreEntCanCollideFromSpec(ent);
    RestoreEntDrawLayerFromSpec(ent);
    ent.grounded = false;
    RemoveEffect(ent, EffectId::NoGravityUntilContact);
}

void ReleaseEntFromHolderIfAttached(Ent& ent, State& state) {
    if (!HasAttachReference(ent, state)) {
        return;
    }
    ReleaseEntFromHolder(ent, state);
}

std::vector<VID> SeverEntCarryLinksForReset(Ent& ent, State& state) {
    std::vector<VID> changed_ents;

    const std::optional<VID> held_vid = ent.holding_vid;
    const std::optional<VID> back_vid = ent.back_vid;
    if (held_vid.has_value()) {
        if (Ent* const held = state.ents.GetEntMut(*held_vid)) {
            RestoreDetachedCarryEnt(*held);
            TrackChangedEnt(changed_ents, held->vid);
        }
    }
    if (back_vid.has_value()) {
        if (Ent* const back = state.ents.GetEntMut(*back_vid)) {
            RestoreDetachedCarryEnt(*back);
            TrackChangedEnt(changed_ents, back->vid);
        }
    }

    const std::optional<VID> holder_vid = ent.held_by_vid;
    if (holder_vid.has_value()) {
        if (Ent* const holder = state.ents.GetEntMut(*holder_vid)) {
            bool holder_changed = false;
            if (holder->holding_vid.has_value() && *holder->holding_vid == ent.vid) {
                holder->holding_vid.reset();
                holder->holding = false;
                holder->holding_timer = kDefaultHoldingTimer;
                holder_changed = true;
            }
            if (holder->back_vid.has_value() && *holder->back_vid == ent.vid) {
                holder->back_vid.reset();
                holder_changed = true;
            }
            if (holder_changed) {
                TrackChangedEnt(changed_ents, holder->vid);
            }
        }
    }

    // Repair asymmetric references too; resets may happen after one side has
    // already been respawned or detached by another gameplay path.
    for (Ent& candidate : state.ents.ents) {
        if (!candidate.active || candidate.vid == ent.vid) {
            continue;
        }

        bool candidate_changed = false;
        if (candidate.holding_vid.has_value() && *candidate.holding_vid == ent.vid) {
            candidate.holding_vid.reset();
            candidate.holding = false;
            candidate.holding_timer = kDefaultHoldingTimer;
            candidate_changed = true;
        }
        if (candidate.back_vid.has_value() && *candidate.back_vid == ent.vid) {
            candidate.back_vid.reset();
            candidate_changed = true;
        }
        if (candidate.held_by_vid.has_value() && *candidate.held_by_vid == ent.vid) {
            RestoreDetachedCarryEnt(candidate);
            candidate_changed = true;
        }
        if (candidate_changed) {
            TrackChangedEnt(changed_ents, candidate.vid);
        }
    }

    ent.holding = false;
    ent.holding_vid.reset();
    ent.back_vid.reset();
    RestoreDetachedCarryEnt(ent);
    TrackChangedEnt(changed_ents, ent.vid);
    return changed_ents;
}

std::vector<VID> SeverEntOutboundCarryLinksForReset(Ent& ent, State& state) {
    std::vector<VID> changed_ents;

    const std::optional<VID> held_vid = ent.holding_vid;
    const std::optional<VID> back_vid = ent.back_vid;
    if (held_vid.has_value()) {
        if (Ent* const held = state.ents.GetEntMut(*held_vid)) {
            RestoreDetachedCarryEnt(*held);
            TrackChangedEnt(changed_ents, held->vid);
        }
    }
    if (back_vid.has_value()) {
        if (Ent* const back = state.ents.GetEntMut(*back_vid)) {
            RestoreDetachedCarryEnt(*back);
            TrackChangedEnt(changed_ents, back->vid);
        }
    }

    for (Ent& candidate : state.ents.ents) {
        if (!candidate.active || candidate.vid == ent.vid) {
            continue;
        }

        bool candidate_changed = false;
        if (candidate.held_by_vid.has_value() && *candidate.held_by_vid == ent.vid) {
            RestoreDetachedCarryEnt(candidate);
            candidate_changed = true;
        }
        if (candidate_changed) {
            TrackChangedEnt(changed_ents, candidate.vid);
        }
    }

    ent.holding = false;
    ent.holding_vid.reset();
    ent.back_vid.reset();
    TrackChangedEnt(changed_ents, ent.vid);
    return changed_ents;
}

void DropHeldItemFromEnt(Ent& ent, State& state) {
    if (!ent.holding_vid.has_value()) {
        return;
    }

    Ent* const held = state.ents.GetEntMut(*ent.holding_vid);
    ent.holding_vid.reset();
    ent.holding = false;
    ent.holding_timer = kDefaultHoldingTimer;
    if (held == nullptr) {
        return;
    }

    if (IsPlayerEnt(*held, state)) {
        const VID dropped_by_vid = ent.vid;
        ReleaseEntFromHolder(*held, state);
        held->vel = FxVec2::zero();
        held->acc = FxVec2::zero();
        state.contact.AddInteractionCooldown(
            ent.vid,
            held->vid,
            InteractionCooldownKind::Harm,
            state.stage_frame,
            kForcedPlayerDropHarmCooldownFrames
        );
        state.contact.AddInteractionCooldown(
            held->vid,
            ent.vid,
            InteractionCooldownKind::Harm,
            state.stage_frame,
            kForcedPlayerDropHarmCooldownFrames
        );
        (void)dropped_by_vid;
        return;
    }

    const FxScalar throw_x =
        FxScalar::from_int(ent.facing == Side::Left ? -3 : 3);
    held->held_by_vid.reset();
    held->attach_mode = AttachMode::None;
    StopUsingEnt(*held);
    RestoreEntHasPhysicsFromSpec(*held);
    RestoreEntCanCollideFromSpec(*held);
    RestoreEntDrawLayerFromSpec(*held);
    held->grounded = false;
    held->thrown_by = ent.vid;
    held->thrown_immunity_timer = kThrownByImmunityDuration;
    const EntSpec& held_spec = GetEntSpec(held->type_);
    held->can_apply_proj_contact = held_spec.can_apply_proj_contact;
    held->proj_contact_damage_type = held_spec.proj_contact_damage_type;
    held->proj_contact_damage_amount = held_spec.proj_contact_damage_amount;
    held->proj_contact_timer = kProjContactDuration;
    held->vel = FxVec2{throw_x, FxScalar::from_int(-1)};
    held->acc = FxVec2::zero();
    RemoveEffect(*held, EffectId::NoGravityUntilContact);
}

bool TryPickupEntByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
) {
    Ent* const holder = state.ents.GetEntMut(holder_vid);
    Ent* const held = state.ents.GetEntMut(held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return false;
    }
    if (holder->holding_vid.has_value()) {
        return false;
    }
    if (!CanPickUpCandidate(*holder, *held, state)) {
        return false;
    }

    holder->holding_timer = kDefaultHoldingTimer;
    AttachEntAsHeld(*holder, *held);
    SyncHeldAttachForHolder(*holder, state, graphics);
    return true;
}

bool TryDropEntByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
) {
    Ent* const holder = state.ents.GetEntMut(holder_vid);
    Ent* const held = state.ents.GetEntMut(held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return false;
    }
    if ((!holder->holding_vid.has_value() || *holder->holding_vid != held_vid) &&
        (!held->held_by_vid.has_value() || *held->held_by_vid != holder_vid)) {
        return false;
    }

    ReleaseEntFromHolder(*held, state);
    held->vel = FxVec2::zero();
    held->acc = FxVec2::zero();
    state.UpdateSidForEnt(held->vid.id, graphics);
    return true;
}

bool TryThrowEntByVid(
    VID thrower_vid,
    VID thrown_vid,
    FxVec2 throw_velocity,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    Ent* const thrower = state.ents.GetEntMut(thrower_vid);
    Ent* const thrown = state.ents.GetEntMut(thrown_vid);
    if (thrower == nullptr || thrown == nullptr || !thrower->active || !thrown->active) {
        return false;
    }
    if ((!thrower->holding_vid.has_value() || *thrower->holding_vid != thrown_vid) &&
        (!thrown->held_by_vid.has_value() || *thrown->held_by_vid != thrower_vid)) {
        return false;
    }

    ApplyThrowState(*thrower, *thrown, throw_velocity, state, graphics, audio);
    return true;
}

bool TryPutHeldEntOnBackByVid(
    VID holder_vid,
    VID held_vid,
    State& state,
    const Graphics& graphics
) {
    Ent* const holder = state.ents.GetEntMut(holder_vid);
    Ent* const held = state.ents.GetEntMut(held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return false;
    }
    if (!holder->holding_vid.has_value() || *holder->holding_vid != held_vid ||
        holder->back_vid.has_value() || !held->can_go_on_back) {
        return false;
    }

    holder->back_vid = held->vid;
    holder->holding_vid.reset();
    holder->holding = false;
    holder->holding_timer = kDefaultHoldingTimer;

    ApplyHeldState(*held);
    held->held_by_vid = holder->vid;
    held->attach_mode = AttachMode::Back;
    held->has_physics = false;
    held->can_collide = false;
    SyncEntAttachs(holder->vid.id, state, graphics);
    return true;
}

bool TryTakeOffBackEntByVid(
    VID holder_vid,
    VID back_vid,
    State& state,
    const Graphics& graphics
) {
    Ent* const holder = state.ents.GetEntMut(holder_vid);
    Ent* const back_item = state.ents.GetEntMut(back_vid);
    if (holder == nullptr || back_item == nullptr || !holder->active || !back_item->active) {
        return false;
    }
    if (!holder->back_vid.has_value() || *holder->back_vid != back_vid) {
        return false;
    }

    back_item->has_physics = true;
    back_item->can_collide = true;
    TrySetAnim(*back_item, EntDisplayState::Neutral);
    back_item->held_by_vid.reset();
    back_item->attach_mode = AttachMode::None;
    StopUsingEnt(*back_item);
    back_item->thrown_by = holder->vid;
    back_item->thrown_immunity_timer = kThrownByImmunityDuration;
    const EntSpec& back_item_spec = GetEntSpec(back_item->type_);
    back_item->can_apply_proj_contact = back_item_spec.can_apply_proj_contact;
    back_item->proj_contact_damage_type = back_item_spec.proj_contact_damage_type;
    back_item->proj_contact_damage_amount = back_item_spec.proj_contact_damage_amount;
    back_item->proj_contact_timer = kProjContactDuration;
    RemoveEffect(*back_item, EffectId::NoGravityUntilContact);

    holder->back_vid.reset();
    state.UpdateSidForEnt(back_item->vid.id, graphics);
    return true;
}

void CleanupInactiveCarryReferences(std::size_t ent_idx, State& state) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& ent = state.ents.ents[ent_idx];
    if (ent.holding_vid.has_value()) {
        const Ent* const holding = state.ents.GetEnt(*ent.holding_vid);
        if (holding == nullptr || !holding->active) {
            ent.holding_vid.reset();
            ent.holding = false;
            ent.holding_timer = kDefaultHoldingTimer;
        }
    }

    if (ent.back_vid.has_value()) {
        const Ent* const back_item = state.ents.GetEnt(*ent.back_vid);
        if (back_item == nullptr || !back_item->active) {
            ent.back_vid.reset();
        }
    }
}

void UpdateCarryAndBackItems(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    CleanupInactiveCarryReferences(ent_idx, state);

    const bool loss_of_control =
        state.ents.ents[ent_idx].condition == EntCondition::Stunned;
    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(
            state.ents.ents[ent_idx],
            state
        );

    if (!loss_of_control) {
        std::optional<VID> thrown_vid;
        std::optional<std::vector<VID>> trying_to_pick_up_these;
        {
            Ent& ent = state.ents.ents[ent_idx];
            if (control.pick_up_drop_pressed) {
                    if (ent.holding_vid.has_value()) {
                        if (ent.holding_timer == 0) {
                            thrown_vid = ent.holding_vid;
                            ent.holding_timer = kDefaultHoldingTimer;
                            ent.holding_vid.reset();
                            ent.holding = false;
                        }
                    } else {
                    if (!ent.IsHanging() && !ent.IsClimbing() && ent.holding_timer == 0) {
                        ent.holding_timer = kDefaultHoldingTimer;
                        const FxAABB aabb = GetContactAabbForEnt(ent, graphics);
                        trying_to_pick_up_these =
                            QueryEntsInAabb(state, aabb, ent.vid);
                    }
                }
            }
            if (ent.holding_timer > 0) {
                ent.holding_timer -= 1;
            }
        }

        std::optional<VID> trying_to_pick_this_up_vid;
        {
            Ent& ent = state.ents.ents[ent_idx];
            if (trying_to_pick_up_these.has_value()) {
                for (const VID& vid : *trying_to_pick_up_these) {
                    const Ent& candidate = state.ents.ents[vid.id];
                    if (!CanPickUpCandidate(ent, candidate, state)) {
                        continue;
                    }
                    trying_to_pick_this_up_vid = vid;
                    break;
                }
            }
        }

        {
            Ent& ent = state.ents.ents[ent_idx];
            if (trying_to_pick_this_up_vid.has_value()) {
                (void)TryPickupEntByVid(
                    ent.vid,
                    *trying_to_pick_this_up_vid,
                    state,
                    graphics
                );
            }
        }

        {
            Ent& ent = state.ents.ents[ent_idx];
            const bool trying_to_go_down = control.down;
            const bool trying_to_go_up = control.up;
            const bool trying_to_go_left = control.left;
            const bool trying_to_go_right = control.right;
            const float mitt_throw_boost =
                ent.grounded && trying_to_go_down
                    ? 0.0F
                    : GetModifiedEffectValue(ent, EffectModifierTarget::ThrowHorizontalBoost, 0.0F);
            const bool mitt_throw = mitt_throw_boost != 0.0F;

            if (thrown_vid.has_value()) {
                if (Ent* const thrown = state.ents.GetEntMut(*thrown_vid)) {
                    FxVec2 throw_vel = FxVec2::zero();
                    if (trying_to_go_left) {
                        throw_vel.x = FxScalar::from_int(-10);
                    } else if (trying_to_go_right) {
                        throw_vel.x = FxScalar::from_int(10);
                    }
                    if (trying_to_go_up) {
                        throw_vel.y = FxScalar::from_int(-10);
                    }
                    if (trying_to_go_down) {
                        throw_vel.y = FxScalar::from_int(10);
                    }
                    if (!trying_to_go_up && !trying_to_go_down &&
                        (trying_to_go_left || trying_to_go_right)) {
                        throw_vel.y = FxScalar::from_int(-2);
                    }
                    if (mitt_throw) {
                        const int throw_direction =
                            throw_vel.x < FxScalar::zero() ? -1 :
                            throw_vel.x > FxScalar::zero() ? 1 :
                            ent.facing == Side::Left ? -1 : 1;
                        throw_vel.x += FxScalar::from_int(throw_direction) *
                                       ToFxScalar(mitt_throw_boost);
                        if (!trying_to_go_up && !trying_to_go_down) {
                            throw_vel.y = ToFxScalar(-0.4F);
                        } else if (trying_to_go_down) {
                            throw_vel.y = FxScalar::from_int(6);
                        }
                    }

                    const FxVec2 scaled_throw_vel =
                        throw_vel * ent.throw_velocity_scale;
                    (void)TryThrowEntByVid(
                        ent.vid,
                        thrown->vid,
                        scaled_throw_vel,
                        state,
                        graphics,
                        audio
                    );
                }
            }
        }
    }

    if (!loss_of_control) {
        std::optional<VID> take_off_back_vid;
        bool put_held_on_back = false;
        bool equip_action_was_made = false;
        {
            const Ent& ent = state.ents.ents[ent_idx];
            if (ent.equip_delay_countdown == 0) {
                if (control.buy_pressed && ent.back_vid.has_value()) {
                    equip_action_was_made = true;
                    take_off_back_vid = ent.back_vid;
                }
                if (control.equip_pressed && ent.holding_vid.has_value()) {
                    const Ent* const held_thing = state.ents.GetEnt(*ent.holding_vid);
                    if (held_thing != nullptr && held_thing->can_go_on_back) {
                        equip_action_was_made = true;
                        put_held_on_back = true;
                        (void)PlayEntCenterSoundEmitter(state, ent, audio_asset_ids::Equip);
                    }
                }
            }
        }

        {
            Ent& ent = state.ents.ents[ent_idx];
            if (equip_action_was_made) {
                ent.equip_delay_countdown = player::kEquipDelay;
            } else if (ent.equip_delay_countdown > 0) {
                ent.equip_delay_countdown -= 1;
            }
        }

        const VID ent_vid = state.ents.ents[ent_idx].vid;

        if (take_off_back_vid.has_value()) {
            (void)TryTakeOffBackEntByVid(ent_vid, *take_off_back_vid, state, graphics);
        }

        {
            Ent& ent = state.ents.ents[ent_idx];
            if (put_held_on_back) {
                const std::optional<VID> held_vid = ent.holding_vid;
                if (held_vid.has_value()) {
                    (void)TryPutHeldEntOnBackByVid(ent.vid, *held_vid, state, graphics);
                }
            }
        }
    }

    {
        const Ent& ent = state.ents.ents[ent_idx];
        if (ent.holding_vid.has_value()) {
            if (Ent* const holding = state.ents.GetEntMut(*ent.holding_vid)) {
                if (control.use_held) {
                    UseEnt(*holding, ent.vid, AttachMode::Held);
                } else {
                    StopUsingEnt(*holding);
                }
            }
        }
        if (ent.back_vid.has_value()) {
            if (Ent* const back_item = state.ents.GetEntMut(*ent.back_vid)) {
                if (control.use_back) {
                    UseEnt(*back_item, ent.vid, AttachMode::Back);
                } else {
                    StopUsingEnt(*back_item);
                }
            }
        }
    }
}

void SyncEntAttachs(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics
) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    CleanupInactiveCarryReferences(ent_idx, state);
    const Ent& holder = state.ents.ents[ent_idx];
    if (!holder.active) {
        return;
    }

    SyncHeldAttachForHolder(holder, state, graphics);
    SyncBackAttachForHolder(holder, state, graphics);
}

} // namespace splonks::ents::common

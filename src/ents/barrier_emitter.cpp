#include "ents/barrier_emitter.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "fxp.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <optional>
#include <vector>

namespace splonks::ents::barrier_emitter {

namespace {

constexpr int kBeamSegmentCount = 3;
constexpr int kBeamSegmentSizePixels = 16;
constexpr std::uint32_t kBeamContactDamage = 1;

bool HasSolidSupportAbove(const Ent& emitter, const State& state) {
    const FxVec2 probe =
        emitter.GetCenter() + FxVec2::from_pixels(0, -kBeamSegmentSizePixels);
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(state.stage, probe);
    return tile_query.has_value() && tile_query->tile != nullptr &&
           IsTileCollidable(*tile_query->tile);
}

std::vector<VID>& EnsureChildBeamVids(Ent& emitter) {
    if (!emitter.child_vids.has_value()) {
        emitter.child_vids.emplace();
    }
    return *emitter.child_vids;
}

Ent* SpawnBeamSegment(State& state, FxVec2 center, const Ent& emitter) {
    return world_ops::SpawnEnt(state, EntType::Beam, [&](Ent& beam) {
        beam.SetCenter(center);
        beam.ent_a = emitter.vid;
        beam.facing = emitter.facing;
        beam.alpha = emitter.alpha;
    });
}

void DestroyBeamChildren(Ent& emitter, State& state) {
    if (!emitter.child_vids.has_value()) {
        return;
    }

    for (const VID& child_vid : *emitter.child_vids) {
        (void)world_ops::DeactivateEnt(state, child_vid);
    }
    emitter.child_vids->clear();
}

void EnsureBeamSegments(std::size_t emitter_idx, State& state) {
    Ent& emitter = state.ents.ents[emitter_idx];
    std::vector<VID>& beam_vids = EnsureChildBeamVids(emitter);
    beam_vids.resize(kBeamSegmentCount);

    for (std::size_t segment_idx = 0; segment_idx < static_cast<std::size_t>(kBeamSegmentCount); ++segment_idx) {
        const FxVec2 segment_center =
            emitter.GetCenter() +
            FxVec2::from_pixels(
                0,
                kBeamSegmentSizePixels * static_cast<int>(segment_idx + 1)
            );
        Ent* beam = state.ents.GetEntMut(beam_vids[segment_idx]);
        if (beam == nullptr || !beam->active || beam->type_ != EntType::Beam ||
            beam->ent_a != emitter.vid) {
            beam = SpawnBeamSegment(state, segment_center, emitter);
            if (beam == nullptr) {
                beam_vids[segment_idx] = VID{};
                continue;
            }
            beam_vids[segment_idx] = beam->vid;
        }

        beam->SetCenter(segment_center);
        beam->facing = emitter.facing;
        beam->alpha = emitter.alpha;
    }
}

void StepEntLogicAsBarrierEmitter(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& emitter = state.ents.ents[ent_idx];
    if (!emitter.active || emitter.condition == EntCondition::Dead) {
        return;
    }

    if (!HasSolidSupportAbove(emitter, state)) {
        emitter.health = 0;
        common::DieIfDead(ent_idx, state, audio);
        return;
    }

    EnsureBeamSegments(ent_idx, state);
}

void StepEntLogicAsBeam(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& beam = state.ents.ents[ent_idx];
    if (!beam.ent_a.has_value()) {
        (void)world_ops::DeactivateEnt(state, beam.vid);
        return;
    }

    const Ent* const emitter = state.ents.GetEnt(*beam.ent_a);
    if (emitter == nullptr || !emitter->active || emitter->condition == EntCondition::Dead) {
        (void)world_ops::DeactivateEnt(state, beam.vid);
    }
}

void OnDeathAsBarrierEmitter(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& emitter = state.ents.ents[ent_idx];
    DestroyBeamChildren(emitter, state);
    AddShake(
        state,
        ToFVec2(emitter.GetCenter()),
        1.4F,
        2.0F,
        ShakeMask::ForegroundTiles | ShakeMask::BackgroundTiles | ShakeMask::Ents
    );
}

} // namespace

extern const EntSpec kBarrierEmitterSpec{
    .type_ = EntType::BarrierEmitter,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::AnthingExceptJumpOn,
    .death_sound = audio_asset_ids::BombExplosion,
    .on_death = OnDeathAsBarrierEmitter,
    .step_logic = StepEntLogicAsBarrierEmitter,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::BeamEmitter),
};

extern const EntSpec kBeamSpec{
    .type_ = EntType::Beam,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = true,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .proj_contact_damage_amount = kBeamContactDamage,
    .can_apply_proj_contact = false,
    .step_logic = StepEntLogicAsBeam,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Beam),
};

} // namespace splonks::ents::barrier_emitter

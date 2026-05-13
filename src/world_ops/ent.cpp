#include "world_ops.hpp"

#include "audio.hpp"
#include "buying.hpp"
#include "ent.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "world_query.hpp"

namespace splonks::world_ops {

namespace {

bool AreEntsOverlappingForInteract(
    const Ent& source,
    const Ent& target,
    const State& state,
    const Graphics& graphics
) {
    const AABB source_aabb = ents::common::GetContactAabbForEnt(source, graphics);
    const Vec2 source_center = (source_aabb.tl + source_aabb.br) / 2.0F;
    const AABB target_aabb = GetNearestWorldAabb(
        state.stage,
        source_center,
        ents::common::GetContactAabbForEnt(target, graphics)
    );
    return AabbsIntersect(source_aabb, target_aabb);
}

} // namespace

Ent* SpawnConfiguredEnt(
    State& state,
    const EntSpawnSetup& setup,
    std::optional<VID> held_by_vid
) {
    (void)held_by_vid;

    const std::optional<VID> vid = state.ents.NewEnt();
    if (!vid.has_value()) {
        return nullptr;
    }

    Ent* const ent = state.ents.GetEntMut(*vid);
    if (ent == nullptr) {
        return nullptr;
    }

    if (setup) {
        setup(*ent);
    }
    return ent;
}

Ent* SpawnEnt(
    State& state,
    EntType type_,
    const EntSpawnSetup& setup,
    std::optional<VID> held_by_vid
) {
    return SpawnConfiguredEnt(
        state,
        [&](Ent& ent) {
            SetEntAs(ent, type_);
            if (setup) {
                setup(ent);
            }
        },
        held_by_vid
    );
}

bool DeactivateEnt(State& state, VID ent_vid) {
    Ent* const ent = state.ents.GetEntMut(ent_vid);
    if (ent == nullptr || !ent->active) {
        return false;
    }

    ents::common::ReleaseEntFromHolder(*ent, state);

    state.ents.SetInactive(ent->vid.id);
    return true;
}

bool TryApplyInteractEnt(
    VID source_vid,
    VID target_vid,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    Ent* const source = state.ents.GetEntMut(source_vid);
    Ent* const target = state.ents.GetEntMut(target_vid);
    if (source == nullptr || target == nullptr ||
        !source->active || !target->active ||
        source->condition == EntCondition::Dead) {
        return false;
    }

    if (!AreEntsOverlappingForInteract(*source, *target, state, graphics)) {
        return false;
    }

    if (target->buyable.active) {
        return TryBuyEnt(target->vid.id, source->vid.id, state, graphics, audio);
    }

    const EntSpec& spec = GetEntSpec(target->type_);
    if (spec.on_interact == nullptr) {
        return false;
    }
    return spec.on_interact(target->vid.id, source->vid.id, state, graphics, audio);
}

} // namespace splonks::world_ops

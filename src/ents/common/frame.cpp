#include "ents/common/common.hpp"

#include "aframe_id.hpp"
#include "fxp.hpp"

#include <algorithm>

namespace splonks::ents::common {

namespace {

void ApplyAFrameGeometryToEnt(std::size_t ent_idx, State& state, const Graphics& graphics) {
    Ent& ent = state.ents.ents[ent_idx];
    if (!ent.render_enabled) {
        return;
    }

    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return;
    }

    if (aframe->pbox.w <= 0 || aframe->pbox.h <= 0) {
        return;
    }

    ent.size = FxVec2::from_pixels(aframe->pbox.w, aframe->pbox.h);
}

} // namespace

const AFrame* GetCurrentAFrameForEnt(const Ent& ent, const Graphics& graphics) {
    if (!ent.render_enabled) {
        return nullptr;
    }

    if (!ent.aframe_animator.HasAnim()) {
        const AFrameAnim* const fallback_anim =
            graphics.aframe_db.FindAnim(aframe_ids::NoSprite);
        if (fallback_anim == nullptr || fallback_anim->frame_indices.empty()) {
            return nullptr;
        }
        return &graphics.aframe_db.frames[fallback_anim->frame_indices[0]];
    }

    const AFrameAnim* anim =
        graphics.aframe_db.FindAnim(ent.aframe_animator.anim_id);
    if (anim == nullptr || anim->frame_indices.empty()) {
        anim = graphics.aframe_db.FindAnim(aframe_ids::NoSprite);
        if (anim == nullptr || anim->frame_indices.empty()) {
            return nullptr;
        }
    }

    std::size_t frame_index = static_cast<std::size_t>(ent.aframe_animator.current_frame);
    if (frame_index >= anim->frame_indices.size()) {
        frame_index = 0;
    }

    return &graphics.aframe_db.frames[anim->frame_indices[frame_index]];
}

FxVec2 GetSpriteTopLeftForEnt(const Ent& ent, const AFrame& aframe) {
    const FxVec2 draw_offset =
        FxVec2::from_pixels(aframe.draw_offset.x, aframe.draw_offset.y);
    const FxVec2 pbox_offset = FxVec2::from_pixels(aframe.pbox.x, aframe.pbox.y);

    if (ent.facing == Side::Left) {
        return ent.pos - pbox_offset + draw_offset;
    }

    const int mirrored_pbox_x = aframe.sample_rect.w - aframe.pbox.x - aframe.pbox.w;
    FxVec2 facing_adjusted_draw_offset = draw_offset;
    if (ent.type_ == EntType::BaseballBat) {
        facing_adjusted_draw_offset = FxVec2{-draw_offset.x, draw_offset.y};
    }
    return ent.pos - FxVec2::from_pixels(mirrored_pbox_x, aframe.pbox.y) +
           facing_adjusted_draw_offset;
}

FxVec2 GetVisualCenterForEnt(const Ent& ent, const Graphics& graphics, FxVec2 fallback) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return fallback;
    }

    const FxVec2 sprite_tl = GetSpriteTopLeftForEnt(ent, *aframe);
    const FxVec2 sprite_world_size =
        FxVec2::from_pixels(aframe->sample_rect.w, aframe->sample_rect.h) *
        ent.aframe_animator.scale;
    return sprite_tl + (sprite_world_size / FxScalar::from_int(2));
}

void SetVisualCenterForEnt(Ent& ent, const Graphics& graphics, FxVec2 center) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        ent.SetCenter(center);
        return;
    }

    const FxVec2 draw_offset =
        FxVec2::from_pixels(aframe->draw_offset.x, aframe->draw_offset.y);
    const FxVec2 pbox_size = FxVec2::from_pixels(aframe->pbox.w, aframe->pbox.h);
    const FxVec2 pbox_center_offset =
        (pbox_size - FxVec2::from_pixels(1, 1)) / FxScalar::from_int(2);

    if (ent.facing == Side::Left) {
        ent.pos = center - draw_offset - pbox_center_offset;
        return;
    }

    FxVec2 facing_adjusted_draw_offset = draw_offset;
    if (ent.type_ == EntType::BaseballBat) {
        facing_adjusted_draw_offset = FxVec2{-draw_offset.x, draw_offset.y};
    }
    ent.pos = center - facing_adjusted_draw_offset - pbox_center_offset;
}

FxVec2 GetEmitPointForEnt(const Ent& ent, const Graphics& graphics, FxVec2 fallback) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return fallback;
    }

    const FxVec2 sprite_tl = GetSpriteTopLeftForEnt(ent, *aframe);
    int emit_x = aframe->emit_point.x;
    if (ent.facing == Side::Right) {
        emit_x = aframe->sample_rect.w - 1 - aframe->emit_point.x;
    }

    const FxVec2 emit_point = sprite_tl + FxVec2::from_pixels(
                                                 emit_x,
                                                 aframe->emit_point.y
                                             );
    if (aframe->emit_point.x == 0 && aframe->emit_point.y == 0) {
        return fallback;
    }
    return emit_point;
}

FxAABB GetContactAabbForEnt(const Ent& ent, const Graphics& graphics) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return ent.GetAABB();
    }
    if (aframe->cbox.w <= 0 || aframe->cbox.h <= 0) {
        return ent.GetAABB();
    }

    const FxVec2 sprite_tl = GetSpriteTopLeftForEnt(ent, *aframe);
    int contact_x = aframe->cbox.x;
    if (ent.facing == Side::Right) {
        contact_x = aframe->sample_rect.w - aframe->cbox.x - aframe->cbox.w;
    }

    const FxVec2 contact_tl =
        sprite_tl + FxVec2::from_pixels(contact_x, aframe->cbox.y);
    return FxAABB::from_corners(
        contact_tl,
        contact_tl + FxVec2::from_pixels(aframe->cbox.w - 1, aframe->cbox.h - 1)
    );
}

FxAABB GetEntBroadphaseAabb(const Ent& ent, const Graphics& graphics) {
    const FxAABB pbox = ent.GetAABB();
    const FxAABB cbox = GetContactAabbForEnt(ent, graphics);
    return FxAABB::from_corners(gfxp::min(pbox.tl, cbox.tl), gfxp::max(pbox.br, cbox.br));
}

void StepAnimTimer(std::size_t ent_idx, State& state, const Graphics& graphics, float dt) {
    Ent& ent = state.ents.ents[ent_idx];
    ent.aframe_animator.Step(graphics.aframe_db, dt);
}

void RefreshAllEntAFrameGeometry(State& state, const Graphics& graphics) {
    for (std::size_t ent_idx = 0; ent_idx < state.ents.ents.size(); ++ent_idx) {
        if (!state.ents.ents[ent_idx].active) {
            continue;
        }
        ApplyAFrameGeometryToEnt(ent_idx, state, graphics);
    }
}

void CommonPostStep(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    StepAnimTimer(ent_idx, state, graphics, dt);
    ApplyAFrameGeometryToEnt(ent_idx, state, graphics);
}

} // namespace splonks::ents::common

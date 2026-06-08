#include "ents/common/common.hpp"

#include "aframe_id.hpp"
#include "sim/fxp.hpp"

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

    ent.size = sim::Vec2::from_pixels(aframe->pbox.w, aframe->pbox.h);
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

Vec2 GetSpriteTopLeftForEnt(const Ent& ent, const AFrame& aframe) {
    const Vec2 draw_offset = Vec2::New(
        static_cast<float>(aframe.draw_offset.x),
        static_cast<float>(aframe.draw_offset.y)
    );
    const Vec2 pbox_offset = Vec2::New(
        static_cast<float>(aframe.pbox.x),
        static_cast<float>(aframe.pbox.y)
    );

    if (ent.facing == Side::Left) {
        return ent.GetRenderPos() - pbox_offset + draw_offset;
    }

    const float mirrored_pbox_x =
        static_cast<float>(aframe.sample_rect.w - aframe.pbox.x - aframe.pbox.w);
    Vec2 facing_adjusted_draw_offset = draw_offset;
    if (ent.type_ == EntType::BaseballBat) {
        facing_adjusted_draw_offset = Vec2::New(-draw_offset.x, draw_offset.y);
    }
    return ent.GetRenderPos() - Vec2::New(mirrored_pbox_x, static_cast<float>(aframe.pbox.y)) +
           facing_adjusted_draw_offset;
}

Vec2 GetVisualCenterForEnt(const Ent& ent, const Graphics& graphics, const Vec2& fallback) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return fallback;
    }

    const Vec2 sprite_tl = GetSpriteTopLeftForEnt(ent, *aframe);
    const Vec2 sprite_world_size = Vec2::New(
        static_cast<float>(aframe->sample_rect.w),
        static_cast<float>(aframe->sample_rect.h)
    ) * sim::ToRenderScalar(ent.aframe_animator.scale);
    return sprite_tl + (sprite_world_size * 0.5F);
}

void SetVisualCenterForEnt(Ent& ent, const Graphics& graphics, const Vec2& center) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        ent.SetCenter(center);
        return;
    }

    const Vec2 draw_offset = Vec2::New(
        static_cast<float>(aframe->draw_offset.x),
        static_cast<float>(aframe->draw_offset.y)
    );
    const Vec2 pbox_size = Vec2::New(
        static_cast<float>(aframe->pbox.w),
        static_cast<float>(aframe->pbox.h)
    );
    const Vec2 pbox_center_offset = (pbox_size - Vec2::New(1.0F, 1.0F)) * 0.5F;

    if (ent.facing == Side::Left) {
        ent.SetRenderPos(center - draw_offset - pbox_center_offset);
        return;
    }

    Vec2 facing_adjusted_draw_offset = draw_offset;
    if (ent.type_ == EntType::BaseballBat) {
        facing_adjusted_draw_offset = Vec2::New(-draw_offset.x, draw_offset.y);
    }
    ent.SetRenderPos(center - facing_adjusted_draw_offset - pbox_center_offset);
}

Vec2 GetEmitPointForEnt(const Ent& ent, const Graphics& graphics, const Vec2& fallback) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return fallback;
    }

    const Vec2 sprite_tl = GetSpriteTopLeftForEnt(ent, *aframe);
    float emit_x = static_cast<float>(aframe->emit_point.x);
    if (ent.facing == Side::Right) {
        emit_x = static_cast<float>(aframe->sample_rect.w - 1 - aframe->emit_point.x);
    }

    const Vec2 emit_point =
        sprite_tl + Vec2::New(emit_x, static_cast<float>(aframe->emit_point.y));
    if (aframe->emit_point.x == 0 && aframe->emit_point.y == 0) {
        return fallback;
    }
    return emit_point;
}

AABB GetContactAabbForEnt(const Ent& ent, const Graphics& graphics) {
    const AFrame* const aframe = GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return ent.GetAABB();
    }
    if (aframe->cbox.w <= 0 || aframe->cbox.h <= 0) {
        return ent.GetAABB();
    }

    const Vec2 sprite_tl = GetSpriteTopLeftForEnt(ent, *aframe);
    float contact_x = static_cast<float>(aframe->cbox.x);
    if (ent.facing == Side::Right) {
        contact_x =
            static_cast<float>(aframe->sample_rect.w - aframe->cbox.x - aframe->cbox.w);
    }

    const Vec2 contact_tl = sprite_tl + Vec2::New(contact_x, static_cast<float>(aframe->cbox.y));
    return AABB::New(
        contact_tl,
        contact_tl + Vec2::New(
                         static_cast<float>(aframe->cbox.w),
                         static_cast<float>(aframe->cbox.h)
                     ) -
            Vec2::New(1.0F, 1.0F)
    );
}

AABB GetEntBroadphaseAabb(const Ent& ent, const Graphics& graphics) {
    const AABB pbox = ent.GetAABB();
    const AABB cbox = GetContactAabbForEnt(ent, graphics);
    return AABB::New(
        Vec2::New(std::min(pbox.tl.x, cbox.tl.x), std::min(pbox.tl.y, cbox.tl.y)),
        Vec2::New(std::max(pbox.br.x, cbox.br.x), std::max(pbox.br.y, cbox.br.y))
    );
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

#include "entities/common.hpp"

#include "entity_display_states.hpp"

#include <algorithm>

namespace splonks::entities::common {

namespace {

void ApplyFrameDataGeometryToEntity(std::size_t entity_idx, State& state, const Graphics& graphics) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const FrameData* const frame_data = GetCurrentFrameDataForEntity(entity, graphics);
    if (frame_data == nullptr) {
        return;
    }

    if (frame_data->pbox.w <= 0 || frame_data->pbox.h <= 0) {
        return;
    }

    entity.size = Vec2::New(
        static_cast<float>(frame_data->pbox.w),
        static_cast<float>(frame_data->pbox.h)
    );
}

} // namespace

const FrameData* GetCurrentFrameDataForEntity(const Entity& entity, const Graphics& graphics) {
    if (!entity.frame_data_animator.HasAnimation()) {
        return nullptr;
    }

    const FrameDataAnimation* const animation =
        graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return nullptr;
    }

    std::size_t frame_index = entity.frame_data_animator.current_frame;
    if (frame_index >= animation->frame_indices.size()) {
        frame_index = 0;
    }

    return &graphics.frame_data_db.frames[animation->frame_indices[frame_index]];
}

Vec2 GetSpriteTopLeftForEntity(const Entity& entity, const FrameData& frame_data) {
    const Vec2 draw_offset = Vec2::New(
        static_cast<float>(frame_data.draw_offset.x),
        static_cast<float>(frame_data.draw_offset.y)
    );
    const Vec2 pbox_offset = Vec2::New(
        static_cast<float>(frame_data.pbox.x),
        static_cast<float>(frame_data.pbox.y)
    );

    if (entity.facing == LeftOrRight::Left) {
        return entity.pos - pbox_offset + draw_offset;
    }

    const float mirrored_pbox_x =
        static_cast<float>(frame_data.sample_rect.w - frame_data.pbox.x - frame_data.pbox.w);
    Vec2 facing_adjusted_draw_offset = draw_offset;
    if (entity.type_ == EntityType::BaseballBat) {
        facing_adjusted_draw_offset = Vec2::New(-draw_offset.x, draw_offset.y);
    }
    return entity.pos - Vec2::New(mirrored_pbox_x, static_cast<float>(frame_data.pbox.y)) +
           facing_adjusted_draw_offset;
}

AABB GetContactAabbForEntity(const Entity& entity, const Graphics& graphics) {
    const FrameData* const frame_data = GetCurrentFrameDataForEntity(entity, graphics);
    if (frame_data == nullptr) {
        return entity.GetAABB();
    }
    if (frame_data->cbox.w <= 0 || frame_data->cbox.h <= 0) {
        return entity.GetAABB();
    }

    const Vec2 sprite_tl = GetSpriteTopLeftForEntity(entity, *frame_data);
    float contact_x = static_cast<float>(frame_data->cbox.x);
    if (entity.facing == LeftOrRight::Right) {
        contact_x =
            static_cast<float>(frame_data->sample_rect.w - frame_data->cbox.x - frame_data->cbox.w);
    }

    const Vec2 contact_tl = sprite_tl + Vec2::New(contact_x, static_cast<float>(frame_data->cbox.y));
    return AABB::New(
        contact_tl,
        contact_tl + Vec2::New(
                         static_cast<float>(frame_data->cbox.w),
                         static_cast<float>(frame_data->cbox.h)
                     ) -
            Vec2::New(1.0F, 1.0F)
    );
}

AABB GetEntityBroadphaseAabb(const Entity& entity, const Graphics& graphics) {
    const AABB pbox = entity.GetAABB();
    const AABB cbox = GetContactAabbForEntity(entity, graphics);
    return AABB::New(
        Vec2::New(std::min(pbox.tl.x, cbox.tl.x), std::min(pbox.tl.y, cbox.tl.y)),
        Vec2::New(std::max(pbox.br.x, cbox.br.x), std::max(pbox.br.y, cbox.br.y))
    );
}

void StepAnimationTimer(std::size_t entity_idx, State& state, const Graphics& graphics, float dt) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
        .type_ = entity.type_,
        .display_state = entity.display_state,
    });
    if (selection.has_value()) {
        entity.frame_data_animator.SetAnimation(selection->animation_id);
        entity.frame_data_animator.animate = selection->animate;
        if (selection->has_forced_frame) {
            entity.frame_data_animator.SetForcedFrame(selection->forced_frame);
        }
    }

    entity.frame_data_animator.Step(graphics.frame_data_db, dt);
}

void CommonPostStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    StepAnimationTimer(entity_idx, state, graphics, dt);
    ApplyFrameDataGeometryToEntity(entity_idx, state, graphics);
}

} // namespace splonks::entities::common

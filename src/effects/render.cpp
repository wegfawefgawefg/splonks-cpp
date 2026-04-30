#include "effects/render.hpp"

#include "effects.hpp"
#include "entity.hpp"
#include "frame_data.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace splonks {

namespace {

std::size_t GetEffectOverlayAnimationFrameIndex(
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db,
    std::uint64_t tick
) {
    if (animation.frame_indices.empty()) {
        return 0;
    }

    std::uint64_t total_duration = 0;
    for (std::size_t frame_index : animation.frame_indices) {
        total_duration += static_cast<std::uint64_t>(
            std::max(frame_data_db.frames[frame_index].duration, 1)
        );
    }
    if (total_duration == 0) {
        return 0;
    }

    std::uint64_t local_tick = tick % total_duration;
    for (std::size_t ordered_index = 0; ordered_index < animation.frame_indices.size(); ++ordered_index) {
        const FrameData& frame_data = frame_data_db.frames[animation.frame_indices[ordered_index]];
        const std::uint64_t duration = static_cast<std::uint64_t>(std::max(frame_data.duration, 1));
        if (local_tick < duration) {
            return ordered_index;
        }
        local_tick -= duration;
    }

    return 0;
}

void DrawEffectOverlayFrameDataIconRotated(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    FrameDataId animation_id,
    const Vec2& center,
    const IVec2& size,
    double rotation_degrees
) {
    const FrameDataAnimation* const animation = graphics.frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return;
    }

    const std::size_t ordered_frame_index =
        GetEffectOverlayAnimationFrameIndex(*animation, graphics.frame_data_db, state.scene_frame);
    if (ordered_frame_index >= animation->frame_indices.size()) {
        return;
    }

    const FrameData& frame_data =
        graphics.frame_data_db.frames[animation->frame_indices[ordered_frame_index]];
    SDL_Texture* const texture = graphics.GetFrameDataTexture(frame_data.image_id);
    if (texture == nullptr) {
        return;
    }

    const SDL_FRect src{
        static_cast<float>(frame_data.sample_rect.x),
        static_cast<float>(frame_data.sample_rect.y),
        static_cast<float>(frame_data.sample_rect.w),
        static_cast<float>(frame_data.sample_rect.h),
    };
    const SDL_FRect dst{
        std::round(center.x - (static_cast<float>(size.x) * 0.5F)),
        std::round(center.y - (static_cast<float>(size.y) * 0.5F)),
        static_cast<float>(size.x),
        static_cast<float>(size.y),
    };
    const SDL_FPoint rotation_center{
        static_cast<float>(size.x) * 0.5F,
        static_cast<float>(size.y) * 0.5F,
    };
    SDL_RenderTextureRotated(
        renderer,
        texture,
        &src,
        &dst,
        rotation_degrees,
        &rotation_center,
        SDL_FLIP_NONE
    );
}

std::optional<Vec2> FindDefaultExitCenter(const State& state) {
    const StageExitId default_exit_id = state.stage.FindExitId("default");
    const bool should_match_exit_id = !state.stage.exits.empty();

    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active || entity.type_ != EntityType::BasicExit) {
            continue;
        }
        if (should_match_exit_id && entity.stage_exit_id != default_exit_id) {
            continue;
        }
        return entity.GetCenter();
    }

    return std::nullopt;
}

} // namespace

void RenderEffectWorldOverlays(SDL_Renderer* renderer, const State& state, Graphics& graphics, const Entity& owner) {
    if (!owner.effects.value) {
        return;
    }

    for (std::uint8_t effect_index = 0; effect_index < owner.effects->count; ++effect_index) {
        const EffectInstance& effect = owner.effects->effects[effect_index];
        const EffectArchetype& archetype = GetEffectArchetype(effect.id);
        if (archetype.render_world_overlay == nullptr) {
            continue;
        }
        archetype.render_world_overlay(renderer, state, graphics, owner, effect);
    }
}

void RenderCompassWorldOverlay(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Entity&,
    const EffectInstance&
) {
    const std::optional<Vec2> exit_center = FindDefaultExitCenter(state);
    if (!exit_center.has_value()) {
        return;
    }

    const Vec2 nearest_exit_center =
        graphics.camera.target + GetNearestWorldDelta(state.stage, graphics.camera.target, *exit_center);
    const Vec2 exit_screen = graphics.WcToScreen(nearest_exit_center);
    const Vec2 screen_center = graphics.camera.offset;
    const Vec2 direction = exit_screen - screen_center;
    if ((direction.x * direction.x) + (direction.y * direction.y) < 1.0F) {
        return;
    }

    const float min_x = static_cast<float>(graphics.dims.x) * 0.25F;
    const float max_x = static_cast<float>(graphics.dims.x) * 0.75F;
    const float min_y = static_cast<float>(graphics.dims.y) * 0.25F;
    const float max_y = static_cast<float>(graphics.dims.y) * 0.75F;
    const bool exit_in_safe_rect =
        exit_screen.x >= min_x && exit_screen.x <= max_x &&
        exit_screen.y >= min_y && exit_screen.y <= max_y;
    if (exit_in_safe_rect) {
        const float bob = std::sin(static_cast<float>(state.scene_frame) * 0.08F) * 3.0F;
        const Vec2 exit_marker_screen =
            graphics.WcToScreen(nearest_exit_center + Vec2::New(0.0F, -16.0F)) +
            Vec2::New(0.0F, bob);
        const int arrow_size =
            std::max(16, static_cast<int>(static_cast<float>(graphics.dims.y) * 0.045F));
        DrawEffectOverlayFrameDataIconRotated(
            renderer,
            state,
            graphics,
            frame_data_ids::CompassArrow,
            exit_marker_screen,
            IVec2::New(arrow_size, arrow_size),
            -90.0
        );
        return;
    }

    const Vec2 arrow_center = Vec2::New(
        std::clamp(exit_screen.x, min_x, max_x),
        std::clamp(exit_screen.y, min_y, max_y)
    );

    constexpr float kRadiansToDegrees = 57.29577951308232F;
    const double rotation_degrees =
        static_cast<double>((std::atan2(direction.y, direction.x) * kRadiansToDegrees) - 180.0F);
    const int arrow_size = std::max(16, static_cast<int>(static_cast<float>(graphics.dims.y) * 0.055F));
    DrawEffectOverlayFrameDataIconRotated(
        renderer,
        state,
        graphics,
        frame_data_ids::CompassArrow,
        arrow_center,
        IVec2::New(arrow_size, arrow_size),
        rotation_degrees
    );
}

} // namespace splonks

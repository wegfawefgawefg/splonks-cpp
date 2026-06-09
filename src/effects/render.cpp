#include "effects/render.hpp"

#include "effects.hpp"
#include "ent.hpp"
#include "aframe.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace splonks {

namespace {

std::size_t GetEffectOverlayAnimFrameIndex(
    const AFrameAnim& anim,
    const AFrameDb& aframe_db,
    std::uint64_t tick
) {
    if (anim.frame_indices.empty()) {
        return 0;
    }

    std::uint64_t total_duration = 0;
    for (std::size_t frame_index : anim.frame_indices) {
        total_duration += static_cast<std::uint64_t>(
            std::max(aframe_db.frames[frame_index].duration, 1)
        );
    }
    if (total_duration == 0) {
        return 0;
    }

    std::uint64_t local_tick = tick % total_duration;
    for (std::size_t ordered_index = 0; ordered_index < anim.frame_indices.size(); ++ordered_index) {
        const AFrame& aframe = aframe_db.frames[anim.frame_indices[ordered_index]];
        const std::uint64_t duration = static_cast<std::uint64_t>(std::max(aframe.duration, 1));
        if (local_tick < duration) {
            return ordered_index;
        }
        local_tick -= duration;
    }

    return 0;
}

void DrawEffectOverlayAFrameIconRotated(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    AFrameId anim_id,
    const FVec2& center,
    const IVec2& size,
    double rotation_degrees
) {
    const AFrameAnim* const anim = graphics.aframe_db.FindAnim(anim_id);
    if (anim == nullptr || anim->frame_indices.empty()) {
        return;
    }

    const std::size_t ordered_frame_index =
        GetEffectOverlayAnimFrameIndex(*anim, graphics.aframe_db, state.scene_frame);
    if (ordered_frame_index >= anim->frame_indices.size()) {
        return;
    }

    const AFrame& aframe =
        graphics.aframe_db.frames[anim->frame_indices[ordered_frame_index]];
    SDL_Texture* const texture = graphics.GetAFrameTexture(aframe.image_id);
    if (texture == nullptr) {
        return;
    }

    const SDL_FRect src{
        static_cast<float>(aframe.sample_rect.x),
        static_cast<float>(aframe.sample_rect.y),
        static_cast<float>(aframe.sample_rect.w),
        static_cast<float>(aframe.sample_rect.h),
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

std::optional<FVec2> FindDefaultExitCenter(const State& state) {
    const StageExitId default_exit_id = state.stage.FindExitId("default");
    const bool should_match_exit_id = !state.stage.exits.empty();

    for (const Ent& ent : state.ents.ents) {
        if (!ent.active || ent.type_ != EntType::BasicExit) {
            continue;
        }
        if (should_match_exit_id && ent.stage_exit_id != default_exit_id) {
            continue;
        }
        return ToFVec2(ent.GetCenter());
    }

    return std::nullopt;
}

} // namespace

void RenderEffectWorldOverlays(SDL_Renderer* renderer, const State& state, Graphics& graphics, const Ent& owner) {
    if (!owner.effects.value) {
        return;
    }

    for (std::uint8_t effect_index = 0; effect_index < owner.effects->count; ++effect_index) {
        const EffectInstance& effect = owner.effects->effects[effect_index];
        const EffectSpec& spec = GetEffectSpec(effect.id);
        if (spec.render_world_overlay == nullptr) {
            continue;
        }
        spec.render_world_overlay(renderer, state, graphics, owner, effect);
    }
}

void RenderCompassWorldOverlay(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Ent&,
    const EffectInstance&
) {
    const std::optional<FVec2> exit_center = FindDefaultExitCenter(state);
    if (!exit_center.has_value()) {
        return;
    }

    const FVec2 nearest_exit_center =
        graphics.camera.target + GetNearestWorldDelta(state.stage, graphics.camera.target, *exit_center);
    const FVec2 exit_screen = graphics.WcToScreen(nearest_exit_center);
    const FVec2 screen_center = graphics.camera.offset;
    const FVec2 direction = exit_screen - screen_center;
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
        const FVec2 exit_marker_screen =
            graphics.WcToScreen(nearest_exit_center + FVec2::New(0.0F, -16.0F)) +
            FVec2::New(0.0F, bob);
        const int arrow_size =
            std::max(16, static_cast<int>(static_cast<float>(graphics.dims.y) * 0.045F));
        DrawEffectOverlayAFrameIconRotated(
            renderer,
            state,
            graphics,
            aframe_ids::CompassArrow,
            exit_marker_screen,
            IVec2::New(arrow_size, arrow_size),
            -90.0
        );
        return;
    }

    const FVec2 arrow_center = FVec2::New(
        std::clamp(exit_screen.x, min_x, max_x),
        std::clamp(exit_screen.y, min_y, max_y)
    );

    constexpr float kRadiansToDegrees = 57.29577951308232F;
    const double rotation_degrees =
        static_cast<double>((std::atan2(direction.y, direction.x) * kRadiansToDegrees) - 180.0F);
    const int arrow_size = std::max(16, static_cast<int>(static_cast<float>(graphics.dims.y) * 0.055F));
    DrawEffectOverlayAFrameIconRotated(
        renderer,
        state,
        graphics,
        aframe_ids::CompassArrow,
        arrow_center,
        IVec2::New(arrow_size, arrow_size),
        rotation_degrees
    );
}

} // namespace splonks

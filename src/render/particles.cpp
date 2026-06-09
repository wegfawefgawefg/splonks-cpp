#include "render/particles.hpp"

#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "particles/particle_specs.hpp"
#include "particles/ribbon_particle.hpp"
#include "particles/scripted_particle.hpp"
#include "particles/segmented_sprite_particle.hpp"
#include "particles/sprite_particle.hpp"
#include "render/world_texture.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace splonks {

namespace {

struct VisibleWorldRect {
    FVec2 tl;
    FVec2 br;
};

VisibleWorldRect GetVisibleWorldRect(const Graphics& graphics) {
    return VisibleWorldRect{
        .tl = graphics.camera.target - (graphics.camera.offset / graphics.camera.zoom),
        .br = graphics.camera.target +
              ((ToVec2(graphics.dims) - graphics.camera.offset) / graphics.camera.zoom),
    };
}

int FloorDivByFloat(float value, float divisor) {
    if (divisor <= 0.0F) {
        return 0;
    }
    return static_cast<int>(std::floor(value / divisor));
}

std::vector<FVec2> GetVisibleWrappedRenderOffsets(const Stage& stage, const Graphics& graphics) {
    std::vector<FVec2> offsets;
    offsets.push_back(FVec2::New(0.0F, 0.0F));

    const float stage_width = static_cast<float>(stage.GetWidth());
    const float stage_height = static_cast<float>(stage.GetHeight());
    if ((!stage.WrapsX() || stage_width <= 0.0F) && (!stage.WrapsY() || stage_height <= 0.0F)) {
        return offsets;
    }

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    const int min_copy_x = stage.WrapsX() ? FloorDivByFloat(visible.tl.x, stage_width) : 0;
    const int max_copy_x = stage.WrapsX() ? FloorDivByFloat(visible.br.x, stage_width) : 0;
    const int min_copy_y = stage.WrapsY() ? FloorDivByFloat(visible.tl.y, stage_height) : 0;
    const int max_copy_y = stage.WrapsY() ? FloorDivByFloat(visible.br.y, stage_height) : 0;

    offsets.clear();
    for (int copy_y = min_copy_y; copy_y <= max_copy_y; ++copy_y) {
        for (int copy_x = min_copy_x; copy_x <= max_copy_x; ++copy_x) {
            offsets.push_back(FVec2::New(
                static_cast<float>(copy_x) * stage_width,
                static_cast<float>(copy_y) * stage_height
            ));
        }
    }
    return offsets;
}

const AFrame* GetAnimatedParticleAFrame(
    Graphics& graphics,
    const AFrameAnimator& animator,
    AFrameId fallback_anim_id
) {
    const AFrameId anim_id =
        animator.HasAnim() ? animator.anim_id : fallback_anim_id;
    const std::size_t frame_index =
        animator.HasAnim() ? static_cast<std::size_t>(animator.current_frame) : 0;
    if (anim_id == kInvalidAFrameId) {
        return nullptr;
    }
    return graphics.aframe_db.FindFrame(anim_id, frame_index);
}

void RenderAnimatedParticleSprite(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const std::vector<FVec2>& render_offsets,
    const FVec2& pos,
    const FVec2& size,
    float rotation,
    float alpha,
    bool horizontal_flip,
    ParticleLightingMode lighting_mode,
    const AFrameAnimator& animator,
    AFrameId fallback_anim_id = kInvalidAFrameId,
    float tint_r = 1.0F,
    float tint_g = 1.0F,
    float tint_b = 1.0F
) {
    const AFrame* const aframe =
        GetAnimatedParticleAFrame(graphics, animator, fallback_anim_id);
    if (aframe == nullptr) {
        return;
    }

    SDL_Texture* const texture = graphics.GetAFrameTexture(aframe->image_id);
    if (texture == nullptr) {
        return;
    }

    const SDL_FlipMode flip = horizontal_flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    const FVec2 half_size = size / 2.0F;
    Color3 lighting_color = Color3::White();
    switch (lighting_mode) {
    case ParticleLightingMode::SceneLit:
        lighting_color = SampleForegroundLightColorForRender(state, pos);
        break;
    case ParticleLightingMode::Unlit:
        lighting_color = Color3::White();
        break;
    case ParticleLightingMode::Emissive:
        lighting_color = Color3::White(1.35F);
        break;
    }
    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha * 255.0F));
    SDL_SetTextureColorModFloat(
        texture,
        std::clamp(tint_r * lighting_color.r, 0.0F, 2.0F),
        std::clamp(tint_g * lighting_color.g, 0.0F, 2.0F),
        std::clamp(tint_b * lighting_color.b, 0.0F, 2.0F)
    );
    const SDL_FRect src{
        static_cast<float>(aframe->sample_rect.x),
        static_cast<float>(aframe->sample_rect.y),
        static_cast<float>(aframe->sample_rect.w),
        static_cast<float>(aframe->sample_rect.h),
    };
    for (const FVec2& render_offset : render_offsets) {
        const SDL_FRect dst = WorldRectToScreen(graphics, (pos - half_size) + render_offset, size);
        const SDL_FPoint center{dst.w / 2.0F, dst.h / 2.0F};
        RenderWorldTextureRotated(renderer, graphics, texture, &src, dst, rotation, &center, flip);
    }
    SDL_SetTextureAlphaMod(texture, 255);
    SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
}

void RenderSpriteParticlesForLayer(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    DrawLayer layer
) {
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const SpriteParticle& particle : state.particles.sprite_particles) {
        if (particle.draw_layer != layer || particle.IsFinished()) {
            continue;
        }
        RenderAnimatedParticleSprite(
            renderer,
            state,
            graphics,
            render_offsets,
            particle.pos,
            particle.size,
            particle.rot,
            particle.alpha,
            particle.horizontal_flip,
            particle.lighting_mode,
            particle.aframe_animator,
            kInvalidAFrameId,
            particle.tint_r,
            particle.tint_g,
            particle.tint_b
        );
    }
}

void RenderScriptedParticlesForLayer(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    DrawLayer layer
) {
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const ScriptedParticle& particle : state.particles.scripted_particles) {
        if (particle.draw_layer != layer || particle.IsFinished()) {
            continue;
        }
        RenderAnimatedParticleSprite(
            renderer,
            state,
            graphics,
            render_offsets,
            particle.pos,
            particle.size,
            particle.rot,
            particle.alpha,
            particle.horizontal_flip,
            particle.lighting_mode,
            particle.aframe_animator
        );
    }
}

void RenderRibbonParticlesForLayer(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    DrawLayer layer
) {
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const RibbonParticle& particle : state.particles.ribbon_particles) {
        if (particle.IsFinished()) {
            continue;
        }
        const RibbonParticleSpec* const spec =
            GetRibbonParticleSpec(particle.spec_id);
        if (spec == nullptr || spec->draw_layer != layer) {
            continue;
        }
        for (std::size_t i = 0; i + 1 < particle.point_count; ++i) {
            const FVec2 a = particle.points[i];
            const FVec2 b = particle.points[i + 1];
            const FVec2 diff = b - a;
            const float length = Length(diff);
            if (length <= 0.01F) {
                continue;
            }
            const float rotation = std::atan2(diff.y, diff.x) * (180.0F / 3.14159265F);
            RenderAnimatedParticleSprite(
                renderer,
                state,
                graphics,
                render_offsets,
                (a + b) * 0.5F,
                FVec2::New(length, spec->width),
                rotation,
                particle.alpha,
                false,
                spec->lighting_mode,
                particle.aframe_animator,
                spec->anim_id
            );
        }
    }
}

void RenderSegmentedSpriteParticlesForLayer(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    DrawLayer layer
) {
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const SegmentedSpriteParticle& particle : state.particles.segmented_sprite_particles) {
        if (particle.IsFinished()) {
            continue;
        }
        const SegmentedSpriteParticleSpec* const spec =
            GetSegmentedSpriteParticleSpec(particle.spec_id);
        if (spec == nullptr || spec->draw_layer != layer) {
            continue;
        }
        const float spacing =
            spec->spacing > 0.0F ? spec->spacing : Max(spec->segment_size.x, 1.0F);
        for (std::size_t i = 0; i + 1 < particle.point_count; ++i) {
            const FVec2 a = particle.points[i];
            const FVec2 b = particle.points[i + 1];
            const FVec2 diff = b - a;
            const float length = Length(diff);
            if (length <= 0.01F) {
                continue;
            }
            const FVec2 dir = diff / length;
            const float rotation = std::atan2(diff.y, diff.x) * (180.0F / 3.14159265F);
            for (float distance_along = 0.0F; distance_along < length; distance_along += spacing) {
                const FVec2 center = a + (dir * distance_along);
                RenderAnimatedParticleSprite(
                    renderer,
                    state,
                    graphics,
                    render_offsets,
                    center,
                    spec->segment_size,
                    rotation,
                    particle.alpha,
                    particle.horizontal_flip,
                    spec->lighting_mode,
                    particle.aframe_animator,
                    spec->anim_id
                );
            }
        }
    }
}

} // namespace

void RenderParticlesForLayer(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    DrawLayer layer
) {
    RenderSpriteParticlesForLayer(renderer, state, graphics, layer);
    RenderScriptedParticlesForLayer(renderer, state, graphics, layer);
    RenderRibbonParticlesForLayer(renderer, state, graphics, layer);
    RenderSegmentedSpriteParticlesForLayer(renderer, state, graphics, layer);
}

} // namespace splonks

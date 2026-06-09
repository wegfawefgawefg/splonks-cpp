#include "render/tiles_and_ents.hpp"

#include "ent.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "render/particles.hpp"
#include "render/stone_overlay.hpp"
#include "render/tile_lighting.hpp"
#include "render/world_wrap.hpp"
#include "render/world_texture.hpp"
#include "fxp.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace splonks {

namespace {

FVec2 GetShakeOffset(float shake_pixels) {
    if (shake_pixels <= 0.0F) {
        return FVec2::New(0.0F, 0.0F);
    }

    return FVec2::New(
        rng::RandomFloat(-shake_pixels, shake_pixels),
        rng::RandomFloat(-shake_pixels, shake_pixels)
    );
}

Color3 ClampRenderColor(Color3 color, float min_value = 0.0F, float max_value = 2.0F) {
    color.r = std::clamp(color.r, min_value, max_value);
    color.g = std::clamp(color.g, min_value, max_value);
    color.b = std::clamp(color.b, min_value, max_value);
    return color;
}

Color3 GetEntLightingColor(State& state, const Ent& ent, Graphics& graphics) {
    EnsureStageLighting(state);
    const FVec2 visual_center =
        ToFVec2(ents::common::GetVisualCenterForEnt(ent, graphics, ent.GetCenter()));
    Color3 color = SampleForegroundLightColorForRender(state, visual_center);
    const float self_light = ToFloat(ent.self_light);
    if (self_light > 0.0F) {
        color = color + (ToFColor3(ent.light_color) * self_light);
    }
    return ClampRenderColor(color);
}

std::uint64_t MakeEntRenderSmoothingKey(const VID& vid) {
    return (static_cast<std::uint64_t>(vid.id) << 32U) |
           static_cast<std::uint64_t>(vid.version);
}

void PruneStaleEntRenderSmoothing(Graphics& graphics, std::uint32_t frame) {
    constexpr std::uint32_t kStaleFrameWindow = 120;
    for (auto it = graphics.ent_render_smoothing.begin();
         it != graphics.ent_render_smoothing.end();) {
        if (frame - it->second.last_seen_frame > kStaleFrameWindow) {
            it = graphics.ent_render_smoothing.erase(it);
        } else {
            ++it;
        }
    }
}

FVec2 GetSmoothedEntRenderPos(State& state, Graphics& graphics, const Ent& ent) {
    constexpr float kCorrectionResponse = 0.42F;
    constexpr float kSettledDistance = 0.20F;
    constexpr float kSnapDistance = 32.0F;
    const FVec2 ent_render_pos = ToFVec2(ent.pos);

    if (state.stage_frame <= 1) {
        if (!graphics.ent_render_smoothing.empty() && state.stage_frame <= 1) {
            graphics.ent_render_smoothing.clear();
        }
        return ent_render_pos;
    }

    const bool rollback_this_frame =
        state.net_session.lockstep_rollback_enabled &&
        state.performance_stats.rollback_replay_frames_this_frame > 0;
    const std::uint64_t key = MakeEntRenderSmoothingKey(ent.vid);
    EntRenderSmoothingState& entry = graphics.ent_render_smoothing[key];
    entry.last_seen_frame = state.frame;

    if (!entry.active) {
        entry.render_pos = ent_render_pos;
        entry.active = true;
        return ent_render_pos;
    }

    const FVec2 previous = GetNearestWorldPoint(state.stage, ent_render_pos, entry.render_pos);
    const FVec2 delta = ent_render_pos - previous;
    const float distance = Length(delta);
    if (!rollback_this_frame && !entry.smoothing_active) {
        entry.render_pos = ent_render_pos;
        PruneStaleEntRenderSmoothing(graphics, state.frame);
        return ent_render_pos;
    }
    if (distance > kSnapDistance) {
        entry.render_pos = ent_render_pos;
        entry.smoothing_active = false;
        return ent_render_pos;
    }
    if (distance <= kSettledDistance) {
        entry.render_pos = ent_render_pos;
        entry.smoothing_active = false;
        return ent_render_pos;
    }
    if (rollback_this_frame) {
        entry.smoothing_active = true;
    }
    if (!entry.smoothing_active) {
        entry.render_pos = ent_render_pos;
        return ent_render_pos;
    }

    entry.render_pos = previous + (delta * kCorrectionResponse);
    PruneStaleEntRenderSmoothing(graphics, state.frame);
    return entry.render_pos;
}

} // namespace

void RenderEnts(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    std::vector<std::size_t> draw_queue;
    std::vector<std::size_t> next_draw_queue;
    next_draw_queue.reserve(state.ents.ents.size());
    for (std::size_t i = 0; i < state.ents.ents.size(); ++i) {
        next_draw_queue.push_back(i);
    }

    for (DrawLayer layer : {DrawLayer::Background, DrawLayer::Middle, DrawLayer::Foreground}) {
        draw_queue.clear();
        draw_queue.insert(draw_queue.end(), next_draw_queue.begin(), next_draw_queue.end());
        next_draw_queue.clear();
        for (std::size_t ent_id : draw_queue) {
            const Ent& ent = state.ents.ents[ent_id];
            if (!ent.active || !ent.render_enabled) {
                continue;
            }
            if (ent.draw_layer != layer) {
                next_draw_queue.push_back(ent_id);
                continue;
            }

            const AFrame* const aframe =
                ents::common::GetCurrentAFrameForEnt(ent, graphics);
            if (aframe == nullptr) {
                continue;
            }

            SDL_Texture* const sprite_texture =
                graphics.GetAFrameTexture(aframe->image_id);
            if (sprite_texture == nullptr) {
                continue;
            }

            const FVec2 sprite_world_size = FVec2::New(
                static_cast<float>(aframe->sample_rect.w),
                static_cast<float>(aframe->sample_rect.h)
            );
            const FVec2 sprite_scaled_size =
                sprite_world_size * ToFloat(ent.aframe_animator.scale);
            Ent render_ent = ent;
            render_ent.pos = ToFxVec2(GetSmoothedEntRenderPos(state, graphics, ent));
            const FVec2 render_position =
                ToFVec2(ents::common::GetSpriteTopLeftForEnt(render_ent, *aframe));

            const SDL_FRect src{
                static_cast<float>(aframe->sample_rect.x),
                static_cast<float>(aframe->sample_rect.y),
                static_cast<float>(aframe->sample_rect.w),
                static_cast<float>(aframe->sample_rect.h),
            };
            const SDL_FlipMode flip =
                ent.facing == Side::Right ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            const Uint8 ent_alpha = static_cast<Uint8>(
                std::clamp(ToFloat(ent.alpha), 0.0F, 1.0F) * 255.0F);
            const Color3 ent_brightness = GetEntLightingColor(state, ent, graphics);
            SDL_SetTextureAlphaMod(sprite_texture, ent_alpha);
            SDL_SetTextureColorModFloat(
                sprite_texture,
                ent_brightness.r,
                ent_brightness.g,
                ent_brightness.b
            );
            if (ent.type_ == EntType::BallAndChainBall && ent.ent_a.has_value()) {
                if (const Ent* const attached = state.ents.GetEnt(*ent.ent_a)) {
                    if (attached->active) {
                        SDL_SetRenderDrawColor(renderer, 132, 132, 132, 255);
                        const FVec2 anchor_world = ToFVec2(attached->GetCenter()) +
                                                  FVec2::New(0.0F, (ToFVec2(attached->size).y * 0.5F) - 1.0F);
                        const FVec2 ball_world =
                            GetNearestWorldPoint(state.stage, anchor_world, ToFVec2(ent.GetCenter()));
                        for (const FVec2& render_offset : render_offsets) {
                            const FVec2 anchor_screen = WorldToScreen(graphics, anchor_world + render_offset);
                            const FVec2 ball_screen = WorldToScreen(graphics, ball_world + render_offset);
                            SDL_RenderLine(renderer, anchor_screen.x, anchor_screen.y, ball_screen.x, ball_screen.y);
                            SDL_RenderLine(renderer, anchor_screen.x, anchor_screen.y + 1.0F, ball_screen.x, ball_screen.y + 1.0F);
                        }
                    }
                }
            }
            for (const FVec2& render_offset : render_offsets) {
                const FVec2 shake_offset = GetShakeOffset(ToFloat(ent.shake));
                SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    render_position + render_offset + shake_offset,
                    sprite_scaled_size
                );
                const float rotation = ToFloat(ent.rotation);
                if (std::abs(rotation) <= 0.01F) {
                    RenderWorldTextureRotated(renderer, graphics, sprite_texture, &src, dst, 0.0, nullptr, flip);
                } else {
                    const FVec2 rotation_world =
                        ToFVec2(ents::common::GetVisualCenterForEnt(
                            render_ent,
                            graphics,
                            render_ent.GetCenter()
                        )) + render_offset + shake_offset;
                    const FVec2 rotation_screen = WorldToScreen(graphics, rotation_world);
                    const SDL_FPoint rotation_center{
                        rotation_screen.x - dst.x,
                        rotation_screen.y - dst.y
                    };
                    RenderWorldTextureRotated(
                        renderer,
                        graphics,
                        sprite_texture,
                        &src,
                        dst,
                        rotation,
                        &rotation_center,
                        flip
                    );
                }
                if (ent.stone) {
                    const FAABB stone_overlay_aabb = ToFAABB(ent.GetAABB());
                    RenderStoneEntOverlay(
                        renderer,
                        state,
                        graphics,
                        stone_overlay_aabb.tl + render_offset,
                        stone_overlay_aabb.br - stone_overlay_aabb.tl + FVec2::New(1.0F, 1.0F)
                    );
                }
            }
            SDL_SetTextureAlphaMod(sprite_texture, 255);
            SDL_SetTextureColorModFloat(sprite_texture, 1.0F, 1.0F, 1.0F);
        }
        RenderParticlesForLayer(renderer, state, graphics, layer);
    }
}

} // namespace splonks
